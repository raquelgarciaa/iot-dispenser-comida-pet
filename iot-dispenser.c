#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>
#include <ESP32Servo.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include "time.h"
#include "SPIFFS.h"


const char* motor = "raquel.garcia.109@ufrn.edu.br/motor";
const char* mqtt_botao_intervalo_1 = "raquel.garcia.109@ufrn.edu.br/botaointervalo1";
const char* mqtt_botao_intervalo_2 = "raquel.garcia.109@ufrn.edu.br/botaointervalo2";
const char* mqtt_comida = "raquel.garcia.109@ufrn.edu.br/comidasensor";
const char* mqtt_alerta = "raquel.garcia.109@ufrn.edu.br/alerta";

const char* ntpServer = "a.st1.ntp.br";  // You can change this to your preferred NTP server

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer);

#define SOUND_SPEED 0.034

#define IO_USERNAME  "raquel.garcia.109@ufrn.edu.br"
#define IO_KEY       "30122023"

//const char* ssid = "Wokwi-GUEST";
//const char* password ="";

const char* ssid = "NPITI-IoT";
const char* password ="NPITI-IoT";

const char* mqttserver = "maqiatto.com";
const int mqttport = 1883;
const char* mqttUser = IO_USERNAME;
const char* mqttPassword = IO_KEY;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;


// Saídas digitais
#define LED 26
#define TRIG_PIN_ULTRA 18
#define ECHO_PIN_ULTRA 5 

float duration_us, altura_sensor;

int AREA_BASE = 400, ALTURA = 20;

//Entradas analogicas
#define SERVOPIN 25
Servo servo;
int pos = 0;


//Variavéis
float dist = 0;
const char* HORARIO_1 = "12";
const char* HORARIO_2 = "16";

int is_horario_1 = 0;
int is_horario_2 = 0;


bool abrir = false;
char *horario1;
char *horario2;

String s;

void writeFile(String state, String path) { //escreve conteúdo em um arquivo
  File rFile = SPIFFS.open(path, "a");
  if (!rFile) {
    Serial.println("Erro ao abrir arquivo!");
  }
  else {
    Serial.print("Tamanho");
    Serial.println(rFile.size());
    rFile.println(state);
    Serial.print("Gravou: ");
    Serial.println(state);
  }
  rFile.close();
}

String readFile(String path) {
  Serial.println("Read file");
  File rFile = SPIFFS.open(path, "r+");//r+ leitura e escrita
  if (!rFile) {
    Serial.println("Erro ao abrir arquivo!");
  }
  else {
    Serial.print("----------Lendo arquivo ");
    Serial.print(path);
    Serial.println("  ---------");
    while (rFile.position() < rFile.size())
    {
      s = rFile.readStringUntil('\n');
      s.trim();
      Serial.println(s);
    }
    rFile.close();
    Serial.print("----------Final arquivo ");
    Serial.print(path);
    Serial.println("  ---------");
    
    return s;
  }
}

String readFileFive(String path) {
  Serial.println("Read file");
  File rFile = SPIFFS.open(path, "r"); // r+ leitura e escrita
  if (!rFile) {
    Serial.println("Erro ao abrir arquivo!");
    return "";  // Retorna uma string vazia em caso de erro
  } else {
    Serial.print("----------Lendo arquivo ");
    Serial.print(path);
    Serial.println("  ---------");

    // Buffer para armazenar as últimas 5 linhas
    String lines[5];
    int lineCount = 0;

    // Lê o arquivo linha por linha
    while (rFile.position() < rFile.size()) {
      String s = rFile.readStringUntil('\n');
      s.trim();
      if (lineCount < 5) {
        lines[lineCount] = s;
        lineCount++;
      } else {
        // Desloca as linhas para "guardar" as 5 últimas
        for (int i = 1; i < 5; i++) {
          lines[i - 1] = lines[i];
        }
        lines[4] = s;  // Adiciona a nova linha
      }
    }
    rFile.close();

    // Agora, vamos construir a string com as últimas 5 linhas, numeradas
    String result = "";
    for (int i = 0; i < lineCount; i++) {
      result += String(i + 1) + ". " + lines[i] + "\n";
    }

    return result;
  }
}


void formatFile() {
  Serial.println("Formantando SPIFFS");
  SPIFFS.format();
  Serial.println("Formatou SPIFFS");
}

void openFS(void) {
  if (!SPIFFS.begin()) {
    Serial.println("\nErro ao abrir o sistema de arquivos");
  }
  else {
    Serial.println("\nSistema de arquivos aberto com sucesso!");
  }
}


void setup_wifi() {

  delay(10);
 
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_2dBm);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.setTimeOffset(-10800);
}

void abrir_ou_fechar(bool abrir){
  if(abrir == true){
    for (pos = 180; pos >= 50; pos -= 1) {
      servo.write(pos);    
      delay(15);
  }
}else{
    for (pos = 50; pos <= 180; pos += 1) {
      servo.write(pos);    
      delay(15);  
    }
  }           
}
void callback(char* topic, byte* payload, unsigned int length) {
  
  time_t t=timeClient.getEpochTime();
  Serial.print("Messagem recebida [");
  Serial.print(topic);
  Serial.print("] ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strcmp(topic, motor) == 0 && (char)payload[0] == '1') {
    if(is_horario_1 == 1 || is_horario_2 == 1) {
      client.publish(motor, "0");
      writeFile("Falha ao abrir o motor: Horários automáticos definidos", "/LOGS.txt");
      client.publish(mqtt_alerta, "Horários automáticos definidos. Por favor, desligue estes se quiser usar o motor.");

    } else {
      abrir_ou_fechar(true);
      String strOut= "Abriu manualmente - " + String(day(t))+"/"+ String(month(t)) + "/" + String(year(t)) + " " + String(hour(t)) + ":" + String(minute(t)) + ":" + String(second(t));

      writeFile(strOut, "/LOGS.txt");

    }


  }
  if (strcmp(topic, motor) == 0 && (char)payload[0] == '0' && pos != 180) {
    abrir_ou_fechar(false);
    String strOut= "Fechou manualmente - " + String(day(t))+"/"+ String(month(t)) + "/" + String(year(t)) + " " + String(hour(t)) + ":" + String(minute(t)) + ":" + String(second(t));
    writeFile(strOut, "/LOGS.txt");
  }

  if (strcmp(topic, mqtt_botao_intervalo_1) == 0 && (char)payload[0] == '0') {
    is_horario_1 = 0;
    String strOut= "Usuário desligou horário automatico para 12 horas - " + String(day(t))+"/"+ String(month(t)) + "/" + String(year(t)) + " " + String(hour(t)) + ":" + String(minute(t)) + ":" + String(second(t));
    writeFile(strOut, "/LOGS.txt");

  }  else if (strcmp(topic, mqtt_botao_intervalo_1) == 0 && (char)payload[0] == '1') {
    is_horario_1 = 1;
    String strOut= "Usuário setou horário automatico para 12 horas - " + String(day(t))+"/"+ String(month(t)) + "/" + String(year(t)) + " " + String(hour(t)) + ":" + String(minute(t)) + ":" + String(second(t));
    writeFile(strOut, "/LOGS.txt");

  }

  if (strcmp(topic, mqtt_botao_intervalo_2) == 0 && (char)payload[0] == '0') {
    is_horario_2 = 0;
    String strOut= "Usuário desligou horário automatico para 16 horas - " + String(day(t))+"/"+ String(month(t)) + "/" + String(year(t)) + " " + String(hour(t)) + ":" + String(minute(t)) + ":" + String(second(t));
    writeFile(strOut, "/LOGS.txt");

  } else if (strcmp(topic, mqtt_botao_intervalo_2) == 0 && (char)payload[0] == '1') {
    is_horario_2 = 1;
    String strOut= "Usuário setou horário automatico para 16 horas - " + String(day(t))+"/"+ String(month(t)) + "/" + String(year(t)) + " " + String(hour(t)) + ":" + String(minute(t)) + ":" + String(second(t));
    writeFile(strOut, "/LOGS.txt");
  }

}


void reconnect() {


  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    // Create a random client ID
    String clientId = "ESP32 - Sensores";
    clientId += String(random(0xffff), HEX);
    // Se conectado
    if (client.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("conectado");
      // Publish setup
      client.publish(motor, "0");
      client.publish(mqtt_botao_intervalo_1, "0");
      client.publish(mqtt_botao_intervalo_2, "0");

      // Subscribe motor, botao_intervalo1, botao_intervalo2
      client.subscribe(motor);
      client.subscribe(mqtt_botao_intervalo_1);
      client.subscribe(mqtt_botao_intervalo_2);

    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5s");
      delay(5000);
    }
  }
}

void alerta_comida(int altura) {
  int porcentagem = (altura*100)/ALTURA;

  char s_porcentagem[8];
  
  dtostrf(porcentagem,1,2,s_porcentagem);

  if(porcentagem < 50 && porcentagem >= 25){
    writeFile("Alerta: Comida entre 50 e 25 porcento", "/LOGS.txt");

    client.publish(mqtt_alerta, s_porcentagem);
  }

}
int estado = 0;
void abrir_automatico(char* hora) {
  
  if(strcmp(HORARIO_1, hora) == 0  && is_horario_1 == 1 && estado == 0) {
    writeFile("Abriu: 12 Horas", "/LOGS.txt");

    client.publish(mqtt_alerta, "Abriu! 12 Horas");
    abrir_ou_fechar(true);
    delay(5000);
    abrir_ou_fechar(false);  
    client.publish(mqtt_alerta, "Fechou! 12 Horas");
    writeFile("Fechou: 12 Horas", "/LOGS.txt");
    estado=1;
  } else if (strcmp(HORARIO_2, hora) == 0  && is_horario_2 == 1 && estado == 0) {
    writeFile("Abriu: 16 Horas", "/LOGS.txt");

    client.publish(mqtt_alerta, "Abriu! 16 Horas");
    abrir_ou_fechar(true);
    delay(5000);
    abrir_ou_fechar(false);  
    client.publish(mqtt_alerta, "Fechou! 16 Horas");
    estado = 1;
    writeFile("Fechou: 16 Horas", "/LOGS.txt");
  } else {
    Serial.println("horas não batem");
    Serial.println(HORARIO_1);
    Serial.println(hora);
  }

  //reseta estado
  if(strcmp("13", hora) == 0 || strcmp("17", hora) == 0){
    estado = 0;
  }
}

void setup() {

  
  Serial.begin(115200);

  ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
	servo.setPeriodHertz(50);    // standard 50 hz servo
	servo.attach(SERVOPIN); // attaches the servo on pin 18 to the servo object

  pinMode(TRIG_PIN_ULTRA, OUTPUT);
  pinMode(ECHO_PIN_ULTRA, INPUT);
      
  setup_wifi();
  client.setServer(mqttserver, 1883); // Publicar
  client.setCallback(callback); // Receber mensagem

  openFS();
  Serial.println("ler arquivo");
  readFile("/LOGS.txt");

}

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop();
  timeClient.forceUpdate();

  delay(10000);

  unsigned long now = millis();
  if (now - lastMsg > 2000) {

    time_t t =timeClient.getEpochTime();
    int HORA = hour(t);
    char s_hour[2];

    itoa(HORA, s_hour, 10);

    Serial.println(s_hour);

    abrir_automatico(s_hour);

    digitalWrite(TRIG_PIN_ULTRA, LOW);
    delayMicroseconds(5);
    digitalWrite(TRIG_PIN_ULTRA, HIGH);

    // measure duration of pulse from ECHO pin
    duration_us = pulseIn(ECHO_PIN_ULTRA, HIGH);

    // calculate the distance
    altura_sensor = SOUND_SPEED * duration_us/2;

    alerta_comida(altura_sensor);

    char s_dist[8];

    dtostrf(altura_sensor*400*0.2,1,2,s_dist);

    client.publish(mqtt_comida, s_dist);

    // print the value to Serial Monitor
    Serial.print("Altura: ");
    Serial.print(altura_sensor);
    Serial.println(" cm");

    String result  = readFile("/LOGS.txt");
  }

}

