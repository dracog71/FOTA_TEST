/**
  ******************************************************************************
  * @file    OTA_STREAM.c
  * @author  GRIN Team
  * @version V1.0
  * @date    1-Enero-2018
  * @brief   Actuaizacion por paquetes HTTP
  ****************************************************************************** 
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2019 GRIN</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  **/
/* Include------------------------------------------------------------------*/
#include <Update.h>

/* Defines----------------------------------------------------------------- */
#define RXD2 18
#define TXD2 19
String  RXData;
char    TXData[500];
String  TXString;

#define   pinSimRST             21
#define   pinSimPWR             02
#define   pinStatusSim          32
#define   velocidadSim          115200
#define   timeONSim             150     //ms
#define   timeOFFSim            1350    //ms
#define   timeActivacionSim     4500    //ms
#define   timeResetSim          300     //ms
#define   timeDesactivacionSim  1500    //ms tiempo que tarda el sim en apagar
#define   tiempoEspera          2000    //tiempo espera para comandot AT

/* Definicion de variables */
typedef enum{
	SIM_ERROR,
	SIM_OK,
	SIM_RESPONSE,
	SIM_NONE,
}ERROR_ENUM;

typedef enum{
	INIT_SERVER,
	FIRST_COMMAND,
	SECOND_COMMAND,
	DATA_SERVER,
	END_SERVER,
	NO_ACTIVE,
	ACTIVE,
	TOGGLE,
}SERVER_STATUS_ENUM;

/* Definicion de funcion debug 
   Descomentar para usar la funcion de debugueo */
 #define SERIAL_PRINT_DEBUG

/**
  * @brief  Inicializacion de funciones
  * @param  Ninguno
  * @retval Ninguno
  */
void setup() {
  
  /* Inicializacion del usart */
  Serial.begin(115200);        
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2); 
  
  /* configuracion SIM7000 */
  //confgSIM7000();
  //iniciarSIM7000();
  //resetSIM7000();
  
  /* Inicializacion del GSM */
  get_GSM_READY();
  
  /* Set APN */
  set_APN();
 
  /* Obtiene el binario y actualiza */
  _PRINT_DEBUG("Download.bin");
  get_HTTP_FILE("http://206.189.214.91:8182/buffer.bin");
  //get_HTTP_FILE("https://github.com/dracog71/FOTA_TEST/blob/master/FOTA.bin");
  
}

/**
  * @brief  Inicializacion del GSM
  * @param  Ninguno
  * @retval Ninguno
  */
void get_GSM_READY() {

  int i,j;

  /* Se envia el reset por software */
  delay(3000);
  _PRINT_DEBUG("Se envia el reset por software"); 
  Serial2.println("AT+CFUN=1,1");
  
  /* Comandos iniciales */
  Serial.println("Sending first autobaud AT"); 
  while(!Serial2.available()){
    Serial2.println("AT");
    delay(100);
  }
  for(i=0;i<=20;i++){
    Serial2.println("AT");
    delay(50);
  }
  
  /* Primera lectura */
  //RXData = Serial2.readStringUntil('\n');
  
  /* Espera la sincronizacion con el GSM */
  while(strncmp(RXData.c_str(),"SMS Ready",9)!=0){
    RXData = Serial2.readStringUntil('\n');
    Serial2.println("");
    _PRINT_DEBUG(RXData);
  }

  /* Mensaje de finalizacion */
  _PRINT_DEBUG("Inicializacion terminada");
}

/**
  * @brief  Configuracion del APN
  * @param  Ninguno
  * @retval Ninguno
  */
void set_APN(){
	
	/* Seteo del apn de telcel */
	_PRINT_DEBUG("Comienza el seteo del APN");
	get_SIM_RESPONSE("AT+CMGF=1",SIM_OK);
	get_SIM_RESPONSE("AT+CGATT=1",SIM_OK);
	
	/*sprintf(TXData,"AT+CBANDCFG=\%cCAT-M\%c,1,2,4,5,28",34,34);
	get_SIM_RESPONSE(TXData,SIM_OK);
    get_SIM_RESPONSE("AT+CNMP=38",SIM_OK);
    get_SIM_RESPONSE("AT+CMNB=1",SIM_OK);
    get_SIM_RESPONSE("AT+CPSI?",SIM_RESPONSE);*/
	
	sprintf(TXData,"AT+CSTT=%cinternet.itelcel.com%c,%cwebgprs%c,%cwebgprs2002%c",34,34,34,34,34,34);
	//sprintf(TXData,"AT+CSTT=%cwireless.twilio.com%c,%c%c,%c%c",34,34,34,34,34,34);
	get_SIM_RESPONSE(TXData,SIM_OK);
	get_SIM_RESPONSE("AT+CIICR",SIM_OK);
	sprintf(TXData,"AT+CDNSCFG = %c8.8.8.8%c,%c8.8.4.4%c",34,34,34,34);
	get_SIM_RESPONSE(TXData,SIM_RESPONSE);
	get_SIM_RESPONSE("AT+CIFSR",SIM_RESPONSE);
	
	/* Conexion con el SAPBR */
	sprintf(TXData,"AT+SAPBR=3,1,%cContype%c,%cGPRS%c",34,34,34,34);
	get_SIM_RESPONSE(TXData,SIM_OK);
	sprintf(TXData,"AT+SAPBR=3,1,%cAPN%c,%cinternet.itelcel.com%c",34,34,34,34);
	//sprintf(TXData,"AT+SAPBR=3,1,%cAPN%c,%cwireless.twilio.com%c",34,34,34,34);
	get_SIM_RESPONSE(TXData,SIM_OK);
	get_SIM_RESPONSE("AT+SAPBR=1,1",SIM_OK);
	get_SIM_RESPONSE("AT+SAPBR=2,1",SIM_OK);
	
}

/**
  * @brief  Obtiene un archivo del una dureccion http
  * @param  Ninguno
  * @retval Ninguno
  */
void get_HTTP_FILE(String URL){
	
	int _response = INIT_SERVER;
	
	/* Control sobre el espacio del URL */
	char GET_URL[500];
	int INC_FRAME   = 300032;
	int START_FRAME = 0;
	int END_FRAME   = INC_FRAME;
	
	/* Comandos del primer parametro */
    get_SIM_RESPONSE("AT+HTTPINIT",SIM_OK);
	sprintf(GET_URL,"%s?start=0&size=10",URL.c_str()); 
	sprintf(TXData,"AT+HTTPPARA=%cURL%c,%c%s%c",34,34,34,GET_URL,34);
	get_SIM_RESPONSE(TXData,SIM_OK);
	sprintf(TXData,"AT+HTTPPARA=%cCID%c,1",34,34);
	get_SIM_RESPONSE(TXData,SIM_OK);
	
	/* Si tenemos autorizacion ingresamos */
	if(get_HTTP_ACTION() == 200){
	  
	  int FILE_LENGHT = get_TOTAL_FILE_LENGHT();
	  sprintf(TXData,"Valor total del archivo %i",FILE_LENGHT);
	  Serial.println(TXData);
	  
	  /* Si e tamaño en espacio es menor */
	  if(Update.begin(FILE_LENGHT)){  //UPDATE_SIZE_UNKNOWN
	    while(START_FRAME < FILE_LENGHT){ 

		  /* Obtien el siguiente paquete */
		  if (_get_NEXT_FRAME(URL,START_FRAME,END_FRAME) == 200){
			  
			  Serial.println("START_FRAME = " + String(START_FRAME) + "    END_FRAME = " + String(END_FRAME) + "    INC_FRAME = " + String(INC_FRAME) + "    FILE_LENGHT = " + String(FILE_LENGHT));
			  
			  /* Se almacenan los nuevos valores */
			  if(_saveToUpdate(START_FRAME,INC_FRAME) > 0){
				  
			      /* Se aumenta el contador */
			      START_FRAME = START_FRAME + INC_FRAME;
			  
			      /* Se aumenta el siguiente frame */
			      if((END_FRAME + INC_FRAME) < FILE_LENGHT ){
				    END_FRAME = START_FRAME + INC_FRAME;
			      }
			      else{
				    INC_FRAME = (FILE_LENGHT - END_FRAME);  
				    END_FRAME = FILE_LENGHT;
			      }
			    }
				else{
					START_FRAME = FILE_LENGHT + 100;
				}
		    }
	    }
		
	    /* Se intenta la actualizacion */
	    _Update_BIN();
	  }
	}
	else{
	  _PRINT_DEBUG("Error de conexion");
	}		
}

/**
  * @brief  Obtiene la rerspuesta del HTTPACTION
  * @param  Ninguno
  * @retval Ninguno
  */
int get_HTTP_ACTION(){
	
	int _Response = INIT_SERVER;
	int PASS = NO_ACTIVE;
	
	get_SIM_RESPONSE("AT+HTTPACTION=0",SIM_OK);
	
	/* Se limpia la variable de control */
	RXData = "";
	
	/* Se espera respuesta de lectura */
	while(_Response == INIT_SERVER){
		
	  char c = Serial2.read();
	  
	  if(c == '+'){
		  PASS = ACTIVE;
	  }
	  
	  if(PASS == ACTIVE){
		  RXData +=c;
	  }
  
      /* Espera la sincronizacion con el GSM */
      if(strncmp(RXData.c_str(),"+HTTPACTION:",12) == 0 ){
       _Response = FIRST_COMMAND;
      }
	  
	}
	
	RXData = "";
	
	/* Se reinicia la vaiables de control */
	RXData = Serial2.readStringUntil(',');
	_PRINT_DEBUG(RXData);
	
	/* Se reinicia la vaiables de control */
	RXData = Serial2.readStringUntil(',');
	_PRINT_DEBUG(RXData);

    /* Final de la cadena */
	Serial2.readStringUntil('\n');  
	
	return RXData.toInt();
}

/**
  * @brief  Obtiene el valor total del archivo
  * @param  Ninguno
  * @retval Ninguno
  */
int get_TOTAL_FILE_LENGHT(){
	
	int TOTAL_LENGHT = 0;
	
	Serial2.println("AT+HTTPHEAD");
	
	/* Se espera que llegue al valor  */
	Serial2.readStringUntil(':'); 
	Serial2.readStringUntil(':'); 
	Serial2.readStringUntil(':'); 
	Serial2.readStringUntil(':'); 
	
	/* Se elimina el espacio */
	uint8_t c_1 = Serial2.read();
	
	/* Se obtiene la ongitud total del archivo */
	RXData = Serial2.readStringUntil('\n');  
	TOTAL_LENGHT = RXData.toInt();
	
	/* Se espera a que se vacie el buffer */
	while(Serial2.available()){
		uint8_t c = Serial2.read();
	}
	
	return TOTAL_LENGHT;
}

/**
  * @brief  Obtiene el nuevo frame para actualizar
  * @param  Ninguno
  * @retval Ninguno
  */
int _get_NEXT_FRAME(String URL, int START_FRAME, int END_FRAME){
	
	char GET_URL[500];
	
	sprintf(GET_URL,"%s?start=%i&size=%i",URL.c_str(),START_FRAME,END_FRAME); 
	sprintf(TXData,"AT+HTTPPARA=%cURL%c,%c%s%c",34,34,34,GET_URL,34);
	get_SIM_RESPONSE(TXData,SIM_OK);
	
	return get_HTTP_ACTION();
}

/**
  * @brief  Iteraccion de siuientes valores
  * @param  Ninguno
  * @retval Ninguno
  */
int _saveToUpdate(int START_FRAME, int INC_FRAME){
	
	int INCREMENT = 512;
	int START = 0;
	int END = INCREMENT;
	int SIZE;
	
	while(START < INC_FRAME){
		SIZE = _write_UPDATE(START_FRAME,START,END,INCREMENT);
		
		START = START + INCREMENT;
		START_FRAME = START_FRAME + INCREMENT;
		
	    if((END + INCREMENT) < INC_FRAME ){
          END = START + INCREMENT;
        }
        else{
          INCREMENT = (INC_FRAME - END);  
          END = INC_FRAME;
        }

		if(SIZE > 0){
		}
		else{
			START = INC_FRAME + 100;
		}
	}
	
	return SIZE;
}

/**
  * @brief  Obtiene y almacena los valores de actualizacion
  * @param  Ninguno
  * @retval Ninguno
  */
size_t _write_UPDATE(int START_FRAME, int START,int END,int INCREMENT){
	
    uint8_t BUFFER[600];
	int i;
	int _Response = INIT_SERVER;
	
	/* Se espera un OK */
	/*while(_Response == INIT_SERVER){
		Serial2.println("AT");
	    while(Serial2.available()){
		  char c = Serial2.read();
		  if(c == 'K'){
			_Response = SECOND_COMMAND;
		  }
	    }
		delay(10);
	}*/

    /* Detecta si aun existen datos en el bus */
	while(Serial2.available()){
		char c = Serial2.read();
	}
	
    /* Mensaje en pantalla */
	Serial.println("");
	Serial.println("START = " + String(START_FRAME) + "    INC = " + String(INCREMENT));
	Serial.println("");

    /* comando de la siuiente lectura */
	sprintf(TXData,"AT+HTTPREAD=%i,%i",START,INCREMENT); 
	Serial2.println(TXData);

	/* Comando recibido */
	char c_1;
	int INIT = NO_ACTIVE;
	while(INIT == NO_ACTIVE){
		c_1 = Serial2.read();
		if(c_1 == ':'){
			INIT = ACTIVE;
		}
	}

	/* Se obtienen los primeros bytes */
	for(i=0;i<6;i++){
		int8_t c = Serial2.read();
	}

	/* Se resetea la variable */
	i = 0;

	/* Thread de prueba */
	while(i < INCREMENT){
		int8_t c = Serial2.read();
		BUFFER[i] = c;
		i++;
		if(i < INCREMENT){
			while(!Serial2.available());
		}
	}
	
	/* Se obtiene la ongitud total del archivo */
	//delay(10);

	/* Impresion del buffer */
	for(i=0;i<INCREMENT;i++){
	Serial.write(BUFFER[i]);
	}

	size_t written = Update.write(BUFFER,INCREMENT);
	Serial.println("Escrito " + String(written));

	return written;

}

/**
  * @brief  Intenta la actualizacion
  * @param  Ninguno
  * @retval Ninguno
  */
void _Update_BIN(){
	
    if (Update.end()) {
        Serial.println("OTA terminado!");
        if (Update.isFinished()) {
          Serial.println("Actualizacion completada,reiniciando...");
		  ESP.restart();
        }
        else {
          Serial.println("Actualizacion no terminada, algo salio mal!");
        }
    }
    else {
        Serial.println(" Error #: " + String(Update.getError()));
    }
}

/**
  * @brief  Loop principal
  * @param  Ninguno
  * @retval Ninguno
  */
void loop() {

	
	/* Envio para SIM */
	while(Serial.available()){
	  Serial2.write(Serial.read());
	}
	
	/* Recepcion por serial 1 */
	while(Serial2.available()){
	  Serial.write(Serial2.read());
	}
}  

/**
  * @brief  Control sobre el resultado del SIM
  * @param  Ninguno
  * @retval Ninguno
  */
uint32_t get_SIM_RESPONSE(String _toSend, int _waitOK){
  
  int _Response = NO_ACTIVE;
   
  /* Comando enviado */
  _PRINT_DEBUG("get_SIM_RESPONSE");
  _PRINT_DEBUG(_toSend);
	
  /* Send AT data */
  RXData = "";
  Serial2.println(_toSend);

  /* Primera lectura */
  RXData = Serial2.readStringUntil('\n');
  
  /* Espera la sincronizacion con el GSM */
  while(strncmp(RXData.c_str(),_toSend.c_str(),strlen(_toSend.c_str())) != 0 ){
    RXData = Serial2.readStringUntil('\n');
  }
  
  _PRINT_DEBUG(RXData);
  RXData = "";
  
  if(_waitOK == SIM_OK){
	  
	/* Primera lectura */
    RXData = Serial2.readStringUntil('\n');
  
    /* Espera la sincronizacion con el GSM */
    while(strncmp(RXData.c_str(),"OK",2) != 0 ){
      RXData = Serial2.readStringUntil('\n');

	  if (strncmp(RXData.c_str(),"ERROR",5) == 0 ){
		  _PRINT_DEBUG("ERROR");
		  while(1);
	  }
	  else{
		  _PRINT_DEBUG(RXData);
	  }
    }
  }
  else if(_waitOK == SIM_RESPONSE){
    while (_Response == NO_ACTIVE) {
		
	  if(Serial2.available()){
        char c = Serial2.read();
		
		if((c != '\r') && (c != '\n')){
          RXData += c;
	    }
	    else if(RXData.length() >= 4){
		  _Response = ACTIVE;
	    }
	  }
    }
  }
  
  _PRINT_DEBUG(RXData);
  _PRINT_DEBUG("SIM_OK");
  
  return SIM_OK;
}

/**
  * @brief  Reporta el dato por serial para debugeo
  * @param  string a enviar
  * @retval ninguno
  */
void _PRINT_DEBUG(String _toDEBUG)
{ 
  /* El usuario puede agregar su propia implementacion de reporte de nombre de archivo y linea,
     ejemplo: printf("Valor de parametro incorrecto : archivo %s en linea %d\r\n", file, line) */
     
  #ifdef  SERIAL_PRINT_DEBUG

    /* Funcion de envio */
    Serial.print(_toDEBUG); 
    Serial.println("");
    
  #endif
}

  void confgSIM7000(){
    pinMode(  pinSimPWR     , OUTPUT );
    pinMode(  pinSimRST     , OUTPUT );
    pinMode(  pinStatusSim  , INPUT  );

    digitalWrite( pinSimRST , HIGH  ); // Default state
    digitalWrite( pinSimPWR , HIGH  );
    delay(100);
  }

 void iniciarSIM7000(){
    bool  statusSim = digitalRead( pinStatusSim );

    //if( statusSim ){
      apagarSIM7000();
      encerderSIM7000();
    //}else{
      //encerderSIM7000();
    //}

    Serial.println("SIM7000 iniciado");
}
  
  void encerderSIM7000(){
    digitalWrite( pinSimPWR ,HIGH );
    delay(100);    
    digitalWrite(pinSimPWR, LOW);
    delay( timeONSim );
    digitalWrite(pinSimPWR, HIGH);
    delay(timeActivacionSim);

      Serial.println("SIM7000 encendido");
  } 
  
void apagarSIM7000(){
    digitalWrite( pinSimPWR ,HIGH );
    delay(100);
    digitalWrite( pinSimPWR ,LOW );
    delay(timeOFFSim);
    digitalWrite( pinSimPWR ,HIGH );
    delay(timeDesactivacionSim);

      Serial.println("SIM7000 apagado");
  }

  void resetSIM7000(){
    digitalWrite( pinSimRST ,HIGH );
    delay(100);
    digitalWrite( pinSimRST ,LOW );
    delay(timeResetSim);
    digitalWrite( pinSimRST ,HIGH );
    delay(timeActivacionSim);

      Serial.println("SIM7000 reset");
  }

/******************* (C) COPYRIGHT 2019 GRIN  *****FIN DE ARCHIVO****/












