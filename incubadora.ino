/*//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////   INCUBADORA WIFI   ////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

                                              OpenLab Madrid
                                                21/07/2017
                                             CC-BY-NC-SA-3.0



                                                
INDICE

#INCLUDES
#DEFINES

OBJETOS

VARIABLES
  Variables de control de la temperatura
  Variables de la interfaz gráfica
  Variables de control del encoder
  Variables de subida a internet
   

FUNCIONES
  void setup()
    Inicio de la lcd
    Configuracion de los pines
    Creación de las interrupciones del encoder
    INTERRUPCIONES DE LOS TIMERS
      Timer1
      Timer2
    Inicio del sensor
    Inicio del esp8266 (wifi)
  Funciones de control de la temperatura
    floaf ajustartiempo
  Funciones de las interrupciones por encoder
    void desplazar
    void click
  Funciones de los Timers
    Timer1
    Timer2
  Funciones de los menús
    void menu0
    void menu1
  void loop ()
  void draw()
  


/////////////////////////////////////////////////////////////////////////////////////////////////////////*/



#include "OLMWifi.h"
#include <DHT.h>  
#include <Adafruit_Sensor.h>




//MACROS
#define TIMER1ON  TIMSK1=(0<<7)|(0<<6)|(0<<5)|(0<<4)|(0<<3)|(0<<OCIE1B)|(1<<OCIE1A)|(1<<TOIE1)
#define TIMER1OFF TIMSK1=(0<<7)|(0<<6)|(0<<5)|(0<<4)|(0<<3)|(0<<OCIE1B)|(0<<OCIE1A)|(0<<TOIE1)
#define TIMER2ON  TIMSK2=(0<<7)|(0<<6)|(0<<5)|(0<<4)|(0<<3)|(0<<OCIE2B)|(0<<OCIE2A)|(1<<TOIE2)
#define TIMER2OFF TIMSK2=(0<<7)|(0<<6)|(0<<5)|(0<<4)|(0<<3)|(0<<OCIE2B)|(0<<OCIE2A)|(0<<TOIE2)
//Bombilla
#define BOMBILLA 8
#define CICLO 5000 
//LCD 
#define CS 10
#define SCK 13
#define MOSI 11
//Encoder
#define CLICK 2
#define ENCA  3
#define ENCB 4
//Beep
#define beep 5
//Sensor 
#define DHTPIN 12    
#define DHTTYPE DHT11

//Objetos
U8GLIB_ST7920_128X64_1X u8g(13, 11, 10); //Objeto de la LCD SCK//MOSI//CS
DHT dht(DHTPIN, DHTTYPE); //Sensor

//Variables de control de la temperatura
  float e; //error
  int objetivo = 37; //temperatura objetivo
  float T; //% del Tiempo del ciclo apagada
  float t = 0; //temperatura leida por el sensor
  bool flagBomb = false; //el flag que nos dice si la bombilla tiene que encender (false) o apagar (true)
  bool flagat = true; //"flag actualizar tiempo" Ajusta el tiempo de encendido y apagado de la bombilla
//Variables de la interfaz gráfica
  String buf=""; //Buffers que guardan lo que se mostrará en pantalla
  String buf2="";
  int menu=0; //Selector del menú
  int datos [101];
//Variables de control del encoder
  bool flagEnc = true; //¿Puede el encoder gestionar el siguiente valor recibido? Ayuda a hacer una breve pausa entre lectura y lectura del encoder para evitar errores
  int espera = 0; //ajustar el tiempo hasta siguiente pulsación
//Variables de los Timers
  int contador = 0; //contadores para indicar cuantas veces a saltado cada interrupción del timer
  unsigned int contador2 = 0;
//Variables de subida a internet
  bool flagwifi2 = false; //¿Hay que enviar algo por wifi?
  bool flagwifi = false; //¿Estamos conectados al wifi?

void setup() {

  //configuramos la lcd
  u8g.setFont(u8g_font_04b_03r);
  u8g.setColorIndex(1);

  //configuracion de los pines
  pinMode(BOMBILLA, OUTPUT);
  pinMode(beep,OUTPUT);
  pinMode(CLICK,INPUT_PULLUP);
  pinMode(ENCA, INPUT_PULLUP);
  pinMode(ENCB,INPUT);
  pinMode(beep, OUTPUT);

  //Creamos las interrupciones del encoder
  attachInterrupt(1, desplazar, LOW);
  attachInterrupt(0, clik, FALLING);


  //---------------------------------------INTERRUPCIONES DE LOS TIMERS--------------------------------------------------
  
  //TIMER 2 PARA GESTIONAR BOTONES
  
  //apagamos las interrupciones hasta que acabe la configuración
  noInterrupts(); 
  //Activación y preparación de la interrupcion del TIMER2 para que funcione a 1 Hz (una vez por segundo)
  TCCR2A = (1<<COM2A1) | (0<<COM2A0) | (0<<COM2B1)| (0<<COM2B0) | (0<<3) | (0<<2) | (0<<WGM21) | (0<<WGM20);  ;   //modo normal, sin pwm, sin cuenta
  TCCR2B = (0<<FOC2A) | (0<<FOC2B) | (0<<5) | (0<<4) | (0<<WGM22) | (1<<CS22) | (1<<CS21) | (1<<CS20);
  TIMSK2 &= ~(1<<OCIE2A); //quitamos comparación, solo queremos por overflow
  // Oscilador interno
  ASSR = (0<<7) | (0<<EXCLK) | (0<<AS2) | (0<<TCN2UB) | (0<<OCR2AUB) | (0<<OCR2BUB) | (0<<TCR2AUB) | (0<<TCR2BUB) ;
  // configuramos un prescaler de 1024
  TCCR2B |= (1 << CS22) | (1 << CS21) | (1 << CS20);    

  //TIMER 1 para el tiempo de la bombilla encendida y apagada
  //Activación y preparación de la interrupcion del TIMER1
  TCCR1A = 0; //Ponemos estos dos registro a 0, más a delante activremos los Bits que nos interesan
  TCCR1B = 0;
  TCNT1  = 0;//Valor del contador a 0

  OCR1A = 62496;//Valor del registro de comparación = 4 segundos
  
  TCCR1B |= (1 << WGM12); //Mode de desbordamiento por comparación

  TCCR1B |= (1 << CS12) | (1 << CS10);  //Prescaler

  TIMSK1 |= (1 << OCIE1A);//Habilitamos la interrupcion por comparacion
  interrupts();


  //------------------------------------------------------------------------------------------------------------------------

  //Iniciamos el sensor
  dht.begin();
  t = dht.readTemperature();
  datos [0] = t;
  
  //Inicialización del esp8266
  Serial.begin(9600);
  flagwifi = inicioWifi (&u8g);
}
//------------------------------Funciones de control de la temperatura-----------------------------------------------------

float ajustartiempo (void)
{
  t = dht.readTemperature();

  for (int i = 99;i>1;i--)
  {
    datos[i] = datos[i-1];
  }
  datos[0] = t;
  
  e = (float)objetivo-t;
  T = (float)(1-(e/9));
  if (T>1)
    T=1;
  if (T<0)
    T=0;
  
  flagat = false;
  TCNT1  = 0;//Valor del contador a 0
  TIMER1ON; //iniciamos el timer 
}

//------------------------------Funciones de las interrupciones por encoder------------------------------------------------

void desplazar() {
  if (flagEnc && menu == 1)
  {
    noInterrupts();
    if (digitalRead(ENCB) == LOW) {
      if (objetivo > 0)
        objetivo--;
    } else {
      if (objetivo < 65)
      objetivo++;
    }  
    flagEnc = false; //Iniciamos la rutina para que se pueda leer solo despues de que el timer funcione
    espera = 7;
    interrupts();
    TIMER2ON; //Iniciamos el timer 
  }
  delay(50);
}


void clik() {
  if (flagEnc) // que se pueda pulsar solo una vez cada segundo
  {
    noInterrupts();
    //digitalWrite(beep, HIGH);
    switch (menu)
    {
     case (0):
       menu = 1;
      break;
      case(1):
       menu = 0;
      break;
    }
    flagEnc = false;
    espera = 10;
    interrupts();
    TIMER2ON; //Iniciamos el timer 
    
    }
  
  
}


//------------------------Funciones de los Timers----------------------------

//Timer 2 
ISR(TIMER2_OVF_vect){
  if (contador < espera)
  {
    contador = contador + 1;
  } 
  else  //ha pasado un segundo
  {
    contador = 0;
    flagEnc = true;
    TIMER2OFF; //apagamos el timer 
    digitalWrite(beep,LOW);  //Si estaba sonando el beep beep lo paro  
  }
}

//Timer 1

//La idea de este timer es que ocurra la siguiente secuencia

//TIEMPO DE APAGADO DE BOMBILLA -> TIEMPO DE ENCENDIDO DE BOMBILLA -> Fin del ciclo. Se para TIMER1. flagat = true ¡NECESITAMOS REAJUSTAR EL TIEMPO! -> Ajustar tiempo. flagat = false. TIMER1ON. TIEMPO AJUSTADO -> TIEMPO DE APAGADO DE BOMBILLA

ISR(TIMER1_COMPA_vect){
  TIMER1OFF; //apagamos el timer 

  if (!flagat)
  {
    if (flagBomb) //empezamos un nuevo ciclo si toca (flagBomb)
    {
       if (contador2 < 80)
       {
          contador2++;
          
       } else {
           contador2 = 0;
           flagwifi2 = true;
       }
    
     digitalWrite(BOMBILLA,HIGH); //TIEMPO APAGADO
     OCR1A = ((int)62496*(1-T))+1; //Ajustamos el punto donde salta la interrupción.OJO! EL +1 ES IMPORTANTE PARA AJUSTAR LA INTERRUPCION, COMO SALGA 0 ENTRAS EN UN BLUCLE CHUNGO!
     flagBomb = false;
     TCNT1  = 0;//Valor del contador a 0
     TIMER1ON; //iniciamos el timer 
  
    } else {
      
        digitalWrite(BOMBILLA,LOW); //TIEMPO ENCENDIDO
       OCR1A = ((int)62496*(T))+1;
        flagBomb = true;
        flagat = true;

       
    }
     
  }
  
  
}


//------------------------ Funciones de los menús ---------------------------
void menu0 (void)
{
  buf = "Temperatura:" + String((int)t);
  buf2 = "Objetivo:" + String(objetivo);
}

void menu1 (void)
{
  buf = "Nuevo objetivo: " + String(objetivo);
  buf2 = "";
}


//---------------------------------------------------------------------------


void loop() {

    //Gestion de la subida de datos
    if (flagwifi2) //Hay datos que enviar!
    {

      if (!enviardatos(objetivo, t))
        {
          flagwifi = false; //El wifi a fallado, ponemos la flag que lo indica de acuerdo
          flagwifi2 = false;  //Se ha gestionado el envío, quitamos la flag que indica que hay que gestionarlo
        }
        else 
          flagwifi=true;  //El wifi ha ido bien!
          flagwifi2 = false; //Se ha gestionado el envío, quitamos la flag que indica que hay que gestionarlo
    }

    //Ajuste del tiempo de la bombilla
    if (flagat)
      ajustartiempo();

    //Ajuste del menú
    switch (menu)
    {
      case 0:
        menu0();
        break;
      case 1:
        menu1();
        break;
    }
    
    //dibujo de la interfaz
    u8g.firstPage();  
    do {
    draw();
    } while( u8g.nextPage() );
}

void draw () {
  u8g.drawStr (0,15,buf.c_str());
  u8g.drawStr (0,5,buf2.c_str());
  if (flagwifi)
    u8g.drawXBMP( 118, 0, WIFI_width, WIFI_height, WIFI_bits);
  if (menu == 0)
  {
    for (int i = 0;i<101; i++)
     {
      u8g.drawStr (4,64,"0");
      u8g.drawLine(15,64,19,64);
      u8g.drawStr (4,54,"10");
      u8g.drawLine(15,54,19,54);
      u8g.drawStr (4,44,"20");
      u8g.drawLine(15,44,19,44);
      u8g.drawStr (4,34,"30");
      u8g.drawLine(15,34,19,34);
      u8g.drawStr (4,24,"40");
      u8g.drawLine(15,24,19,24);

      u8g.drawLine(19,64,19,24);//eje y
      u8g.drawLine(20+i,63,20+i,63-datos[i]);//medida
     }
  }
  
}
