#define buzzer_pin 2  
#define DownPin 4   // концевики
#define UpPin 15    // Джампер
#define servoPin1 3
#define servoPin2 5 //Дальний от платы Ардуино
#define UpButton 16
#define DownButton 17

#include <Servo.h> 
#include <SPI.h>                                              
#include <SD.h>                                               
#include <SFE_BMP180.h>  //необходимо докачать
#include <Wire.h>
#include <TimerOne.h>
SFE_BMP180 pressure;     // Объявляем переменную для доступа к SFE_BMP180


float alt, velocity, angle; //alt-altitude
double baseline;
unsigned long StartTime = 0; // Время размыкания джампера 

bool firstSave = false; 
bool secondSave = false;

bool uBMP180 = false;
bool uCARD = false;
bool uMPU = false;

Servo Servo1;
Servo Servo2;

// массивы для послдених 10 измерений времени и высоты,нужны для вычисления скорости
float preTime[10] = {0,0,0,0,0,0,0,0,0,0};  
float prePre[10] = {0,0,0,0,0,0,0,0,0,0};

unsigned time1 = 0;
unsigned time2 = 0;
unsigned long bufTime = 100000;

  

void beep(int num, int delay_)  // функция работы бузера
{
    for(int i=0;i<num;i++)
    {
        analogWrite(buzzer_pin,1000);
        delay(delay_);
        analogWrite(buzzer_pin,0);
        if(i!=num-1)delay(delay_);
    }
}



double getPressure()
{
    char status;
    double T,P;

    // Необходимо для работы барометра
    status = pressure.startTemperature();
    if (status != 0)
    {
        delay(status);
        status = pressure.getTemperature(T);
        if (status != 0)
        {
            status = pressure.startPressure(3);
            if (status != 0)
            {
                delay(status);
                status = pressure.getPressure(P,T);
                P = P * 0.750064;
                if (status != 0)
                {
                    Serial.println("");
                    return(P);
                }
            }
        }
    }
}


File logfile;
 char filename[] = "000.txt"; //Первоначальное название 



void setup()
{
   // Задаём начальные установки
  
    Serial.begin(9600);
    Serial.println("Start");
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode (DownPin, INPUT_PULLUP);
    pinMode (buzzer_pin, OUTPUT);
    pinMode(servoPin2, OUTPUT); 
    pinMode(servoPin1, OUTPUT);
    pinMode(UpButton, INPUT); 
    pinMode(DownButton, INPUT);

    Servo1.attach(servoPin1);
    Servo2.attach(servoPin2);
    beep(1,50);
    Wire.begin();
    

//Проверяем работоспособность датчиков
    if (SD.begin(10)) 
    {
        uCARD = true;
    }
 
    if(pressure.begin()) 
    {
        uBMP180 = true;
    }

    if (!uBMP180)beep(2,50); // 2 бипа если отлетел BMP
    if (!uCARD)beep(3,50);  // 3 бипа если отлетел СД
    else beep(1,50);        //1 бип если все норм
    




    

//Проверяем свободные имена
    for (uint8_t i = 0; i < 1000; i++)
    {
        filename[0] = i / 100 + '0';
        filename[1] = i / 10 + '0';
        filename[2] = i % 10 + '0';

        if (! SD.exists(filename)) 
        {
            break;
        }   
    }
    
    logfile  =  SD.open(filename, FILE_WRITE); //Определение давление на уровне места старта
    baseline = getPressure();
    for(int i = 0; i <= 9; i++) prePre[i] = baseline;
    logfile.print(baseline); 
    logfile.println("   ");
    logfile.close();


while(digitalRead(15)) { //проверка стартового джампера

 //Работа кнопок для вращения сервомоторов + отладочное сообщение
    if( digitalRead(UpButton) == LOW){  
        Serial.println("Up---");
        Servo1.write(0);
    }
       
    if( digitalRead(UpButton) == HIGH){
        Serial.println("Up+++");
        Servo1.write(90);
    } 
         
    if( digitalRead(DownButton) == LOW){  
        Serial.println("Do---");
        Servo2.write(0);
    }
       
    if( digitalRead(DownButton) == HIGH){
        Serial.println("Do+++");
        Servo2.write(90);
    } 

}
  //  НАЧАЛО ПОЛЕТА t+0.   Создание нового файла на SD карте, запись в него времени старта + отладка
    logfile  =  SD.open(filename, FILE_WRITE); 
    StartTime = millis();
    for(int i = 0; i <= 9; i++) preTime[i] = StartTime;
    logfile.println(StartTime);
    logfile.println("START-----------------");
    beep (1,50);
    logfile.close();
}


// Функция записи данных с датчиков на SD карту
void info(){    

    logfile  =  SD.open(filename, FILE_WRITE);
    for (byte i = 9; i>0; i--){ preTime[i] = preTime[i-1];}
    for (byte i = 9; i>0; i--){ prePre[i] = prePre[i-1];}
    prePre[0] = getPressure();
    preTime[0] = millis();
    alt = (baseline - prePre[0])*12;
    velocity = (prePre[9] - prePre[0])*12/(preTime[0]/1000 - preTime[9]/1000); // вычисление скорости по средней 10 измерений
    logfile.print(velocity); 
    Serial.print(velocity);
    logfile.print("   ");
    logfile.print(preTime[0]); 
    logfile.print("   ");
    logfile.print(alt);
    logfile.print("   ");
    logfile.println(prePre[0]);
    logfile.close();
}



void loop(){
    
    while (100000 > (millis()-StartTime)){ // время полёта
            
        if (millis() - preTime[0] > 50)               // ОПРОС ДАТЧИКОВ после 50мс
            info();
             
        if(millis()-StartTime > 4000 && velocity < 10 && firstSave == 0){//Основная система спасения. Условие спасения по скорости
            logfile  =  SD.open(filename, FILE_WRITE);
            logfile.print("VelocitySave"); 
            logfile.print(millis());
            bufTime = millis();
            Serial.println("StartSaveSystemVelocity");
            logfile.println("StartSaveSystemVelocity");
            logfile.close();
                for(int i = 0; i < 5; i++){
                    Servo1.write(90);
                    delay(500);
                    info();
                }
            firstSave = 1;
        }
             
        if((millis()-StartTime) > 10500 && firstSave == 0){   // Основная система спасения. Условие спасения по времени
            logfile  =  SD.open(filename, FILE_WRITE);
            logfile.print("StartSaveSystemTime");
            logfile.print(millis());
            logfile.println("StartSaveSystemTime");
            bufTime = millis();
            logfile.close();
                for(int i = 0; i < 5; i++){
                    Servo1.write(90);
                    delay(500);
                    info();
                }
            firstSave = 1;
        }


        if (secondSave == 0 && firstSave == 1 && digitalRead(DownPin) == LOW && (millis()-bufTime > 2500)){//дополнительная система спасения по концевику
            Serial.println("StartReserveSaveSystemEndcap");
            logfile  =  SD.open(filename, FILE_WRITE);         
            logfile.println("StartReserveSaveSystemEndcap");
            logfile.print(millis());
            logfile.close();
            Servo2.write(90);
            delay(500);
        }
             
              
        if (secondSave == 0 && firstSave == 1 && velocity < (-15) && (millis()-bufTime > 3000)){//дополнительная система cпасения по скорости
            Serial.println("StartReserveSaveSystemVelocity");
            logfile  =  SD.open(filename, FILE_WRITE);
            logfile.println("StartReserveSaveSystemVelocity");
            logfile.print(millis());
            logfile.close();
            Servo2.write(90);
            delay(500);
        }

    }
    beep(1,15000000);      // Поиск по звуку    
}
                           