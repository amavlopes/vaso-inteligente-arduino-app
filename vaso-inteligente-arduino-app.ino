#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <time.h>

#include <Wire.h>
#include <BH1750.h>
#include "SparkFunHTU21D.h"
#include <LiquidCrystal_I2C.h>

#define nomeRede "APT 105_2G"
#define senhaRede "Luna1310"
#define urlBancoDados "https://planta-iot-esp32-rtdb-default-rtdb.firebaseio.com/"
#define chaveApi "AIzaSyAF-RxgSc2SKYeMtdiGwRfsf5oBHDCVdwY"

int pinoUmidadeSolo = 32;
int pinoRele = 27;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

BH1750 sensorLuz;
HTU21D sensorTempUmid;
LiquidCrystal_I2C tela(0x27, 16,2);

byte daysavetime = 1;
long timezone = -3;

float luminosidade;
float temperatura;
float umidadeAr;
float umidadeSolo;

unsigned long tempo = 0;
unsigned long tempoBombaDagua = 0;
unsigned long intervalo = 3000;

bool entradaComSucesso = false;

int indice = 0;

String data;
String hora;
String statusUmidadeSolo;
String nivelUmidade[3] = {
  "low", 
  "medium", 
  "high"
};

void setup() {
  definirPinos();

  Wire.begin();
  Serial.begin(115200);
  configurarDataEHora();

  conectarAoFirebase();
  inicializarSensorLuminosidade();
  inicializarSensorTempUmid();
  inicializarTela();
}

void loop() { 

  if( Firebase.ready() && entradaComSucesso && (millis() - tempo > intervalo || tempo == 0) ) {
    tempo = millis();
    definirDataEHora();

    realizarLeituraLuminosidade();
    realizarLeituraTemperaturaEUmidade();
    realizarLeituraUmidadeSolo();

    exibirLeituraTela();
    exibirLeituraMonitorSerial();

    gravarRegistros();

    controlarBombaDAgua();
  }
  
}

void definirPinos() {
  pinMode(pinoRele, OUTPUT);
  digitalWrite(pinoRele, HIGH);
}

void configurarDataEHora() {
  configTime(3600 * timezone, daysavetime * 3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
}

void definirDataEHora() {
  struct tm tmstruct ;

  tmstruct.tm_year = 0;
  getLocalTime(&tmstruct);

  data = (String((tmstruct.tm_year) + 1900) + "-" + String(( tmstruct.tm_mon) + 1) + "-" + String(tmstruct.tm_mday));
  hora = (String(tmstruct.tm_hour) + ":" + String(tmstruct.tm_min) + ":" + String(tmstruct.tm_sec));
}

void conectarAoFirebase() {
  // Conectar a rede Wifi
  WiFi.begin(nomeRede, senhaRede);
  Serial.println("Conectando à rede Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Conectado ao IP: ");
  Serial.println(WiFi.localIP());

  config.api_key = chaveApi;
  config.database_url = urlBancoDados;

  // sign up
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Entrada realizada com sucesso");
    entradaComSucesso = true;
  } else {
    Serial.printf("Erro ao conectar Wi-fi %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.reconnectNetwork(true);
  
  fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);

  config.timeout.networkReconnect = 10 * 1000;
  config.timeout.socketConnection = 10 * 1000;
  config.timeout.serverResponse = 10 * 1000;
  config.timeout.rtdbKeepAlive = 45 * 1000;
  config.timeout.rtdbStreamReconnect = 1 * 1000;
  config.timeout.rtdbStreamError = 3 * 1000;
}

void inicializarSensorLuminosidade() {
  sensorLuz.begin();
  if (sensorLuz.begin()) {
    Serial.println(F("Sensor luminosidade BH1750 inicializado"));
  } else {
    Serial.println(F("Erro ao iniciar sensor luminosidade BH1750"));
  }
};

void inicializarSensorTempUmid() {
  sensorTempUmid.begin();
};

void inicializarTela() {
  tela.init();
  tela.clear();
  tela.backlight();
}

void realizarLeituraLuminosidade() {
  luminosidade = sensorLuz.readLightLevel();
}

void realizarLeituraTemperaturaEUmidade() {
  temperatura = sensorTempUmid.readTemperature();
  umidadeAr = sensorTempUmid.readHumidity();
}

void realizarLeituraUmidadeSolo() {
  umidadeSolo = 100 - ((analogRead(pinoUmidadeSolo)/4095.00) * 100);
  definirStatusUmidadeSolo();
}

void definirStatusUmidadeSolo() {
  if (umidadeSolo > 65) {
    statusUmidadeSolo = nivelUmidade[2];
  } else if (umidadeSolo <= 65 && umidadeSolo >= 30) {
    statusUmidadeSolo = nivelUmidade[1];
  } else {
    statusUmidadeSolo = nivelUmidade[0];
  }
}

void exibirLeituraMonitorSerial() {

  Serial.println();
  Serial.println("******************** INÍCIO **********************");
  Serial.println();
  Serial.println("Data/hora: " + data + " " + hora);
  Serial.println();

  Serial.print("Luminosidade: "); 
  Serial.println(luminosidade);
  Serial.println();

  Serial.print("Temperatura: "); 
  Serial.print(temperatura); 
  Serial.println("°C");
  Serial.println();

  Serial.print("Umidade do ar: "); 
  Serial.print(umidadeAr);
  Serial.println("%");
  Serial.println();

  Serial.print("Umidade solo: "); 
  Serial.print(umidadeSolo);
  Serial.println("%");
  Serial.println();
}

void exibirLeituraTela() {
  tela.clear();

  tela.setCursor(0, 0);
  tela.print("L:"); 
  tela.print(luminosidade);

  tela.setCursor(9, 0);
  tela.print("T:"); 
  tela.print(temperatura); 

  tela.setCursor(0, 1);
  tela.print("Ar:"); 
  tela.print(umidadeAr);

  tela.setCursor(8, 1);
  tela.print("U:"); 
  tela.print(statusUmidadeSolo); 
}

void gravarRegistros() {
  FirebaseJson registro;
  registro.add("dataLeitura", data);
  registro.add("horaLeitura", hora);
  registro.add("luminosidade", luminosidade);
  registro.add("temperatura", temperatura);
  registro.add("umidadeAr", umidadeAr);
  registro.add("umidadeSolo", umidadeSolo);

  if (Firebase.RTDB.set(&fbdo, "/sensores/registros/" + String(indice), &registro)) {
    Serial.println("Registro adicionado em: " + fbdo.dataPath());
    Serial.println();
    Serial.println("********************** FIM ****************************");
    indice++;
  } else {
    Serial.println("Erro ao adicionar registro: " + fbdo.dataPath() + ". " + fbdo.errorReason());
  }
}

void controlarBombaDAgua() {
  if (umidadeSolo <= 30) {
    digitalWrite(pinoRele, LOW);
  } 
  delay(600);
  digitalWrite(pinoRele, HIGH);
}