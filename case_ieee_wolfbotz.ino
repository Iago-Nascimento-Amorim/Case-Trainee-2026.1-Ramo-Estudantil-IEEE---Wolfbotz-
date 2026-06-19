// Case IEEE Wolfbotz
// Atualizado em 07/06/2026 - 02:22

//Como que se faz um código organizado sem parecer IA?
//Bibliotecas
#include "BluetoothSerial.h"
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

//Bluetooth (Melhor manter in English porque fica igual ao app)
BluetoothSerial SerialBT;
#define FORWARD 'F'
#define BACKWARD 'B'
#define LEFT 'L'
#define RIGHT 'R'
#define CIRCLE 'C'
#define CROSS 'X'
#define TRIANGLE 'T'
#define SQUARE 'S'
#define START 'A'
#define PAUSE 'P'

// PINOS -----
//
//Motor A
const int PWMA = 14;
const int AIN1 = 12;
const int AIN2 = 13;
//Motor B
const int BIN1 = 25;
const int BIN2 = 26;
const int PWMB = 27;
//
//Sonar Esquerdo
const int ESQ_TRIG  = 5;
const int ESQ_ECHO  = 18;
//Sonar Direito
const int DIR_TRIG = 23;
const int DIR_ECHO = 22;
// ----------------------

// Velocidades (Em PWM)
const int v_controle = 150;
const int v_estacionamento = 83; // ~6cm/s
const float v = 5.5f;

// VARIÁVEIS GLOBAIS -----
//
// Handles das tarefas (como se fossem os "controles remotos" de cada tarefa)
TaskHandle_t TaskHandle_Bluetooth;
TaskHandle_t TaskHandle_SonarEsq;
TaskHandle_t TaskHandle_SonarDir;
TaskHandle_t TaskHandle_Estacionamento;
// Semáforo para evitar interferência entre os sonares
SemaphoreHandle_t semaforoSonar;
//
// Variáveis do controle
volatile char modoAtual = CIRCLE; // Começa no modo controle remoto por padrão
//
// Variáveis da Busca
volatile bool vaga_encontrada = false;
const float s = 0.44f;
volatile float S = 0.0f;
volatile float A_total = 0.0f;
const float tolerancia_ruido = 0.3f;
volatile float theta_esq = 100.0f;
volatile float theta_dir = 100.0f;
volatile float theta = 100.0f;
volatile float lado_vaga = 0.0f; // -1 esquerda, 1 direita
volatile float dentro_da_vaga = -1.0; // -1 não está, 1 está
volatile float extra = 0.0f; // Serve pro estacionamento
volatile float contra_extra = 0.0f; // Age em dupla com 'extra'. Eu não tinha um bom nome :(

//Declaração das funções
void TaskBluetoothControl(void *pvParameters);
void TaskBuscaEsquerda(void *pvParameters);
void TaskBuscaDireita(void *pvParameters);
void TaskEstacionamento (void *pvParameters);

////


void setup() {
  Serial.begin(115200);
  SerialBT.begin("Robozinho lindo da Equipe 6");  // Nome do Bluetooth

  //pinMode motores e sonares
  pinMode(PWMA, OUTPUT); pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT); pinMode(PWMB, OUTPUT);
  pinMode(ESQ_TRIG, OUTPUT); pinMode(ESQ_ECHO, INPUT);
  pinMode(DIR_TRIG, OUTPUT); pinMode(DIR_ECHO, INPUT);

  semaforoSonar = xSemaphoreCreateMutex(); //Semáforo Mutex

  // TAREFAS | Parâmetros: (Função, Nome, Tamanho da Pilha, Parâmetro, Prioridade, Handle, Núcleo)
  // Todo mundo no núcleo 1
  xTaskCreatePinnedToCore(TaskBluetoothControl, "TaskBT", 4096, NULL, 2, &TaskHandle_Bluetooth, 1);
  xTaskCreatePinnedToCore(TaskBuscaEsquerda, "TaskEsq", 4096, NULL, 1, &TaskHandle_SonarEsq, 1);
  xTaskCreatePinnedToCore(TaskBuscaDireita, "TaskDir", 4096, NULL, 1, &TaskHandle_SonarDir, 1);
  xTaskCreatePinnedToCore(TaskEstacionamento, "TaskEst", 4096, NULL, 1, &TaskHandle_Estacionamento, 1);
}

void loop() {
  // só encerra o void loop
  vTaskDelete(NULL); 
}

void delaytask(unsigned long tempo_ms) {
  unsigned long t_ini = millis();
  while(millis() - t_ini < tempo_ms) {
    if (modoAtual == CIRCLE) {
      // Para os motores
      digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW); analogWrite(PWMA, 0);
      digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW); analogWrite(PWMB, 0);
      return;   // ← volta pro for(;;) sem matar a task
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Mantém a tarefa viva
  }
}

////


// Bluetooth e o Controle Remoto
void TaskBluetoothControl(void *pvParameters) {
  for (;;) {
    if (SerialBT.available()) {
      char comando = SerialBT.read();
      
      if (comando == SQUARE || comando == CIRCLE) {
        modoAtual = comando; 
        
        // Se voltamos pro controle remoto, resetamos a busca da vaga
        if (modoAtual == CIRCLE) {
          vaga_encontrada = false;
        }
      }

      if (comando == SQUARE) {
        digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
        analogWrite(PWMA, v_estacionamento);
        digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
        analogWrite(PWMB, v_estacionamento);
        modoAtual=SQUARE;
      }

      if (modoAtual == CIRCLE) {
        switch (comando) {
          case FORWARD:
            //(Motor A)
            digitalWrite(AIN1, HIGH);
            digitalWrite(AIN2, LOW);
            analogWrite(PWMA, v_controle);
            //(Motor B)
            digitalWrite(BIN1, HIGH);
            digitalWrite(BIN2, LOW);
            analogWrite(PWMB, v_controle);  
            break;
          case BACKWARD:
            //(Motor A)
            digitalWrite(AIN1, LOW);
            digitalWrite(AIN2, HIGH);
            analogWrite(PWMA, v_controle);
            //(Motor B)
            digitalWrite(BIN1, LOW);
            digitalWrite(BIN2, HIGH);
            analogWrite(PWMB, v_controle);
            break;
          case LEFT:
            //(Motor A)
            digitalWrite(AIN1, LOW);
            digitalWrite(AIN2, HIGH);
            analogWrite(PWMA, v_controle);
            //(Motor B)
            digitalWrite(BIN1, HIGH);
            digitalWrite(BIN2, LOW);
            analogWrite(PWMB, v_controle);
            break;
          case RIGHT:
            //(Motor A)
            digitalWrite(AIN1, HIGH);
            digitalWrite(AIN2, LOW);
            analogWrite(PWMA, v_controle);
            //(Motor B)
            digitalWrite(BIN1, LOW);
            digitalWrite(BIN2, HIGH);
            analogWrite(PWMB, v_controle);
            break;
          default:
            //(Motor A)
            digitalWrite(AIN1, LOW);
            digitalWrite(AIN2, LOW);
            analogWrite(PWMA, v_controle);
            //(Motor B)
            digitalWrite(BIN1, LOW);
            digitalWrite(BIN2, LOW);
            analogWrite(PWMB, v_controle);
            break;
        }
        
      }
      
    }
    vTaskDelay(pdMS_TO_TICKS(20)); //Delayzinho pra não sobrecarregar
  }
}

// Reconhecimento do estacionamento - Sonar Esquerdo
void TaskBuscaEsquerda(void *pvParameters) {
  //Variáveis locais
  float A=0.0f, a1=999.0f, a2=999.0f, a3=999.0f, a4=999.0f;
  int n=1; //contador de passos (começa com 1 por causa da colinearida)
  float pv=0.0f; //pós vértice
  int estado=0; //Não queria colocar mais uma variável, mas não consegui reaproveitar outra 

  for (;;) {
    // Só executa se estiver no modo SQUARE e a vaga ainda não tiver sido encontrada
    if (modoAtual == SQUARE && !vaga_encontrada) {
      
      unsigned long tempo_inicio = millis(); // Mantém o semáforo no tempo certo. Veja o cálculo depois dele.

      // Pega o semáforo para usar o ultrassom
      if (xSemaphoreTake(semaforoSonar, portMAX_DELAY) == pdTRUE) {

        // Disparo e leitura da distância
        digitalWrite(ESQ_TRIG, LOW);
        delayMicroseconds(2);
        digitalWrite(ESQ_TRIG, HIGH);
        delayMicroseconds(10);
        digitalWrite(ESQ_TRIG, LOW);
        long duracao = pulseIn(ESQ_ECHO, HIGH, 5800); // vê até 1m
        float dist = (duracao == 0) ? 999.0f : duracao / 58.0f;

        // Devolve o semáforo para o outro sonar poder usar
        xSemaphoreGive(semaforoSonar);

        a4=a3; a3=a2; a2=a1; a1=dist;

        //Se as 3 variáveis forem legíveis
        if (a1+a2+a3 < 999.0f) {
          // Se a1, a2 e a3 forem colineares (deveria ser =0, mas eu permiti pequenos ruídos)
          if (std::abs(a1 - 2.0f * a2 + a3) < tolerancia_ruido) {

            n = n+1; //contador de passos
            
            // Define A pela 1a vez
            if (A==0.0f){
              A = a3;
            }

            if (n>9) {
              // Define theta
              theta_esq = (atanf(std::abs((a1-A))/((float)n*s))*(180.0f/M_PI)); //Mede o módulo de theta
              if (A>a4){ // Conserta o módulo para o valor real
                theta_esq = theta_esq * -1.0;
                if (A>20.0f * std::abs(sinf(theta_esq * M_PI /180.0f))) {dentro_da_vaga = 1.0f;} // Verifica se o carrinho vai entrar na área
              }
              //Se theta passa dos limites utilizáveis
              if ( (theta_esq>45.0f) || (theta_esq<-45.0f) && (A>20.0f * std::abs(sinf(theta_esq * M_PI /180.0f))) ) {
              theta_esq = 100.0f; A=0.0f; a4=999.0f; a3=999.0f; n=1;} //Volta theta pro valor original 
              //Se theta=0 na verdade é theta=-90
              if ((std::abs(theta_esq) < 5.0f) && (std::abs((theta_esq * -1.0f)+theta_dir) < 5.0f)) //5.0f é tolerância pra 0.0f
              {lado_vaga=-1.0f; theta=-90.0f; S=n*s; A_total=(A-(pv*(tanf(theta_esq*M_PI/180.0f)))); vaga_encontrada=true; //Está dentro da vaga 
              xTaskNotifyGive(TaskHandle_Estacionamento); //Vai pra task de estacionar
              } 
              if (theta_esq<0.0f && dentro_da_vaga<0.0f){extra=A;} else {extra=A;} //Serve pra alinhar no estacionamento depois
            }

          } else { //máquina de estado pro robô pegar mais uma aferição da nova reta, e então poder desenhá-la

            if (estado==1){ 
              //Se a medida estiver na faixa de 20±s
              if ( 20.0f+s >= pv + sqrt(pow(n*s, 2.0f) + pow(std::abs(a3-A), 2.0f)) && 20.0f-s <= pv + sqrt(pow(n*s, 2.0f) + pow(std::abs(a3-A), 2.0f)) ) {
                lado_vaga=-1.0f; theta=theta_esq; S=n*s; A_total=(A-(pv*(tanf(theta_esq*M_PI/180.0f)))); estado=0; vaga_encontrada=true; //Encontrou a vaga
                if (theta_esq<0.0f && dentro_da_vaga<0.0f){extra=a3;}
                contra_extra = a3;
                xTaskNotifyGive(TaskHandle_Estacionamento); //Vai pra task de estacionar

                 //Se a reta desenhada for perpendicular
              } else if ( std::abs (90.0f - std::abs(theta_esq) - atanf(std::abs(a1-a2)/s)*(180.0f/M_PI)) < 1.0f ){
                // Calcula as inclinações das duas retas
                float m_antiga = (a3 - a4) / s;
                float m_nova = (a1 - a2) / s; 
                    
                // Calcula a distância horizontal do vértice até a2
                float x_v = (a2 - a3 - (m_nova * s)) / (m_antiga - m_nova);

                // Aplica a fórmula final do pv (Distância na reta)
                pv = std::abs(x_v) * sqrt(1.0f + pow(m_nova, 2.0f));

                // Configura a nova parede
                A = a2;        // a2 se torna o início oficial da nova parede
                n = 1;         // reinicia n pra medir a nova parede  

                estado=0;

              } else {estado = 2;} //Passou pelas opções, mas não é nenhuma delas
            } else {estado = 1;} //Não passou pelas opções, precisa de mais uma verificação

            //Se nenhuma das 2 opções é realidade
            if (estado==2) {
             A=0.0f; a4=999.0f; a3=999.0f; n=1; estado=0; //Reinicia a busca
            }
          } 

          ////Apenas pro caso do a1 depois de uma reta ser 999, adicionei uma verificação pra pegar a medida dela
          if (A != 0.0f) {
            //Como o cara pode encontrar 999 procurando vértice, coloquei isso pra ele saber medir com a2 ou a3
            float a0 = 0.0f;
            if (estado==0){a0=a2;} else {a0=a3;}
            //Se a medida estiver na faixa de 20±s
            if ( 20.0f+s >= pv + sqrt(pow(n*s, 2.0f) + pow(std::abs(a0-A), 2.0f)) && 20.0f-s <= pv + sqrt(pow(n*s, 2.0f) + pow(std::abs(a0-A), 2.0f)) ) {
            lado_vaga=-1.0f; theta=theta_esq; S=n*s; A_total=(A-(pv*(tanf(theta_esq*M_PI/180.0f)))); estado=0; vaga_encontrada=true; //Encontrou a vaga
            if (theta_esq<0.0f && dentro_da_vaga<0.0f){extra=a3;}
            contra_extra = a3;
            xTaskNotifyGive(TaskHandle_Estacionamento); //Vai pra task de estacionar
            }
          }
        }
      }
      // Calcula quanto tempo o sistema usou
      unsigned long tempo_gasto = millis() - tempo_inicio;
      // Mantém o espaço de 80ms constante
      if (tempo_gasto < 80) {
        vTaskDelay(pdMS_TO_TICKS(80 - tempo_gasto)); // Usa apenas o que falta pra chegar a 80ms
      } else {
        vTaskDelay(pdMS_TO_TICKS(1)); // Só de segurança, caso dê MUITO RUIM (nem sei se o cara vai usar)
      }
    }
    // Delay da task desligada
    vTaskDelay(pdMS_TO_TICKS(40)); 
  }
}
       
// Reconhecimento do estacionamento - Sonar Direito
void TaskBuscaDireita(void *pvParameters) {
  //Variáveis locais
  float A=0.0f, a1=999.0f, a2=999.0f, a3=999.0f, a4=999.0f;
  int n=1; //contador de passos (começa com 1 por causa da colinearida)
  float pv=0.0f; //pós vértice
  int estado=0; //Não queria colocar mais uma variável, mas não consegui reaproveitar outra 

  vTaskDelay(40); // Intervalo que SÓ TEM NA DIREITA, pro sensor dele não confundir o de lá

  for (;;) {
    // Só executa se estiver no modo SQUARE e a vaga ainda não tiver sido encontrada
    if (modoAtual == SQUARE && !vaga_encontrada) {
      
      unsigned long tempo_inicio = millis(); // Mantém o semáforo no tempo certo. Veja o cálculo depois dele.

      // Pega o semáforo para usar o ultrassom
      if (xSemaphoreTake(semaforoSonar, portMAX_DELAY) == pdTRUE) {

        // Disparo e leitura da distância
        digitalWrite(DIR_TRIG, LOW);
        delayMicroseconds(2);
        digitalWrite(DIR_TRIG, HIGH);
        delayMicroseconds(10);
        digitalWrite(DIR_TRIG, LOW);
        long duracao = pulseIn(DIR_ECHO, HIGH, 5800); // vê até 1m
        float dist = (duracao == 0) ? 999.0f : duracao / 58.0f;

        // Devolve o semáforo para o outro sonar poder usar
        xSemaphoreGive(semaforoSonar);

        a4=a3; a3=a2; a2=a1; a1=dist;

        //Se as 3 variáveis forem legíveis
        if (a1+a2+a3 < 999.0f) {
          // Se a1, a2 e a3 forem colineares (deveria ser =0, mas eu permiti pequenos ruídos)
          if (std::abs(a1 - 2.0f * a2 + a3) < tolerancia_ruido) {

            n = n+1; //contador de passos
            
            // Define A pela 1a vez
            if (A==0.0f){
              A = a3;
            }

            if (n>9) {
              // Define theta
              theta_dir = (atanf(std::abs((a1-A))/((float)n*s))*(180.0f/M_PI)); //Mede o módulo de theta
              if (A>a4){ // Conserta o módulo para o valor real
                theta_dir = theta_dir * -1.0;
                if (A>20.0f * std::abs(sinf(theta_dir * M_PI /180.0f))) {dentro_da_vaga = 1.0f;} // Verifica se o carrinho vai entrar na área
              }
              //Se theta passa dos limites utilizáveis
              if ( (theta_dir>45.0f) || (theta_dir<-45.0f) && (A>20.0f * std::abs(sinf(theta_dir * M_PI /180.0f))) ) {
              theta_dir = 100.0f; A=0.0f; a4=999.0f; a3=999.0f; n=1;} //Volta theta pro valor original 
              //Se theta=0 na verdade é theta=-90
              if ((std::abs(theta_dir) < 5.0f) && (std::abs((theta_dir * -1.0f)+theta_esq) < 5.0f)) //5.0f é tolerância pra 0.0f
              {lado_vaga=1.0f; theta=-90.0f; S=n*s; A_total=(A-(pv*(tanf(theta_dir*M_PI/180.0f)))); vaga_encontrada=true; //Está dentro da vaga 
              xTaskNotifyGive(TaskHandle_Estacionamento); //Vai pra task de estacionar
              }
              if (theta_dir<0.0f && dentro_da_vaga<0.0f){extra=A;} else {extra=A;} //Serve pra alinhar no estacionamento depois
            }

          } else { //máquina de estado pro robô pegar mais uma aferição da nova reta, e então poder desenhá-la

            if (estado==1){ 
              //Se a medida estiver na faixa de 20±s
              if ( 20.0f+s >= pv + sqrt(pow(n*s, 2.0f) + pow(std::abs(a3-A), 2.0f)) && 20.0f-s <= pv + sqrt(pow(n*s, 2.0f) + pow(std::abs(a3-A), 2.0f)) ) {
                lado_vaga=1.0f; theta=theta_dir; S=n*s; A_total=(A-(pv*(tanf(theta_dir*M_PI/180.0f)))); estado=0; vaga_encontrada=true; //Encontrou a vaga
                if (theta_dir<0.0f && dentro_da_vaga<0.0f){extra=a3;}
                contra_extra = a3;
                xTaskNotifyGive(TaskHandle_Estacionamento); //Vai pra task de estacionar

                 //Se a reta desenhada for perpendicular
              } else if ( std::abs(90.0f - std::abs(theta_dir) - atanf(std::abs(a1-a2)/s)*(180.0f/M_PI)) < 1.0f ){
                // Calcula as inclinações das duas retas
                float m_antiga = (a3 - a4) / s;
                float m_nova = (a1 - a2) / s; 
                    
                // Calcula a distância horizontal do vértice até a2
                float x_v = (a2 - a3 - (m_nova * s)) / (m_antiga - m_nova);

                // Aplica a fórmula final do pv (Distância na reta)
                pv = std::abs(x_v) * sqrt(1.0f + pow(m_nova, 2.0f));

                // Configura a nova parede
                A = a2;        // a2 se torna o início oficial da nova parede
                n = 1;         // reinicia n pra medir a nova parede  

                estado=0;

              } else {estado = 2;} //Passou pelas opções, mas não é nenhuma delas
            } else {estado = 1;} //Não passou pelas opções, precisa de mais uma verificação

            //Se nenhuma das 2 opções é realidade
            if (estado==2) {
             A=0.0f; a4=999.0f; a3=999.0f; n=1;estado=0; //Reinicia a busca
            }
          } 

          ////Apenas pro caso do a1 depois de uma reta ser 999, adicionei uma verificação pra pegar a medida dela
          if (A != 0.0f) {
            //Como o cara pode encontrar 999 procurando vértice, coloquei isso pra ele saber medir com a2 ou a3
            float a0 = 0.0f;
            if (estado==0){a0=a2;} else {a0=a3;}
            //Se a medida estiver na faixa de 20±s
            if ( 20.0f+s >= pv + sqrt(pow(n*s, 2.0f) + pow(std::abs(a0-A), 2.0f)) && 20.0f-s <= pv + sqrt(pow(n*s, 2.0f) + pow(std::abs(a0-A), 2.0f)) ) {
            lado_vaga=-1.0f; theta=theta_dir; S=n*s; A_total=(A-(pv*(tanf(theta_dir*M_PI/180.0f)))); estado=0; vaga_encontrada=true; //Encontrou a vaga
            if (theta_dir<0.0f && dentro_da_vaga<0.0f){extra=a3;}
            contra_extra = a3;
            xTaskNotifyGive(TaskHandle_Estacionamento); //Vai pra task de estacionar
            }
          }
        }
      }
      // Calcula quanto tempo o sistema usou
      unsigned long tempo_gasto = millis() - tempo_inicio;
      // Mantém o espaço de 80ms constante
      if (tempo_gasto < 80) {
        vTaskDelay(pdMS_TO_TICKS(80 - tempo_gasto)); // Usa apenas o que falta pra chegar a 80ms
      } else {
        vTaskDelay(pdMS_TO_TICKS(1)); // Só de segurança, caso dê MUITO RUIM (nem sei se o cara vai usar)
      }
    }
    // Delay da task desligada
    vTaskDelay(pdMS_TO_TICKS(40)); 
  }
}

// Estacionamento
void TaskEstacionamento (void *pvParameters) {
  
  for (;;){
  
    if (modoAtual == SQUARE){
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY); //Espera a task ser ativada pela busca

      //Para o robô
      //(Motor A)
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, LOW);
      analogWrite(PWMA, v_estacionamento);
      //(Motor B)
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, LOW);
      analogWrite(PWMB, v_estacionamento);
      //(Tempo)
      delaytask(500);
      if (modoAtual != SQUARE) continue;
       
      if (theta<-85.0f) {modoAtual = CIRCLE;} // O cabra já estacionou se θ=(-90)

      //Variáveis
      float r = 0.0f;
      if (theta<0.0f && dentro_da_vaga<0.0f){
      r = std::abs(10.0f/cosf(theta*M_PI/180.0f)) - std::abs(extra * tanf(theta*M_PI/180.0f));
      } else {
      r = S - std::abs(10.0f/cosf(theta*M_PI/180.0f)) - std::abs(extra * tanf(theta*M_PI/180.0f));
      }
      int t = (int)(r / v * 1000.0f);   // ticks = milissegundos no ESP32

      //Dá ré até estar no meio do estacionamento e depois para
      //(Motor A)
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, HIGH);
      analogWrite(PWMA, v_estacionamento);
      //(Motor B)
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, HIGH);
      analogWrite(PWMB, v_estacionamento);
      //(Tempo)
      delaytask(t);
      //
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, LOW);
      analogWrite(PWMA, v_estacionamento);
      //(Motor B)
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, LOW);
      analogWrite(PWMB, v_estacionamento);
      //(Tempo)
      delaytask(500);
      if (modoAtual != SQUARE) continue;
      

      //Variáveis para o giro
      float d = r;
      r = ( ((90.0f+theta)*M_PI/180.0f)*4.44f );
      t = (int)(r / v * 1000.0f);
      bool direita = (lado_vaga>0.0f);
      bool esquerda = (lado_vaga<0.0f);


      //Girar (ficar de costas pra parede) e depois para
      //(Motor A)
      digitalWrite(AIN1, esquerda);
      digitalWrite(AIN2, direita);
      analogWrite(PWMA, v_estacionamento);
      //(Motor B)
      digitalWrite(BIN1, direita);
      digitalWrite(BIN2, esquerda);
      analogWrite(PWMB, v_estacionamento);
      //(Tempo)
      delaytask(t);
      //
      //(Motor A)
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, LOW);
      analogWrite(PWMA, v_estacionamento);
      //(Motor B)
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, LOW);
      analogWrite(PWMB, v_estacionamento);
      //(Tempo)
      delaytask(500);
      if (modoAtual != SQUARE) continue;

      //Variáveis pra ré
      if (theta>0.0f) {
        r = 5.0f + sinf(theta*M_PI/180.0f)*(contra_extra-d);
      } else if (dentro_da_vaga<0.0f){
        r = 5.0f + sinf(theta*M_PI/180.0f)*(d+extra);
      } else {
        r = 5.0f - sinf(theta*M_PI/180.0f)*(contra_extra-d);
      }
      t = (int)(r / v * 1000.0f);
      bool pra_tras = (dentro_da_vaga<0.0f);
      bool pra_frente = (dentro_da_vaga>0.0f);

      //Girar (ficar de costas pro estacionamento)
      //(Motor A)
      digitalWrite(AIN1, pra_frente);
      digitalWrite(AIN2, pra_tras);
      analogWrite(PWMA, v_estacionamento);
      //(Motor B)
      digitalWrite(BIN1, pra_frente);
      digitalWrite(BIN2, pra_tras);
      analogWrite(PWMB, v_estacionamento);
      //(Tempo)
      delaytask(t);
      //Para o robô
      //(Motor A)
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, LOW);
      analogWrite(PWMA, v_estacionamento);
      //(Motor B)
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, LOW);
      analogWrite(PWMB, v_estacionamento);

      if (pra_frente == true) {
        if (modoAtual != SQUARE) continue;
        //Variáveis para o giro
        r = ( (M_PI/2.0f)*4.44f );
        t = (int)(r / v * 1000.0f);

        //Girar (ficar de costas pra parede) e depois para
        //(Motor A)
        digitalWrite(AIN1, esquerda);
        digitalWrite(AIN2, direita);
        analogWrite(PWMA, v_estacionamento);
        //(Motor B)
        digitalWrite(BIN1, direita);
        digitalWrite(BIN2, esquerda);
        analogWrite(PWMB, v_estacionamento);
        //(Tempo)
        delaytask(t);
        //
        //(Motor A)
        digitalWrite(AIN1, LOW);
        digitalWrite(AIN2, LOW);
        analogWrite(PWMA, v_estacionamento);
        //(Motor B)
        digitalWrite(BIN1, LOW);
        digitalWrite(BIN2, LOW);
        analogWrite(PWMB, v_estacionamento);
        //(Tempo)
        delaytask(500);
      }
      
      //CHEGOOOU
      modoAtual = CIRCLE; 
      //Pra não repetir a manobra, volta pro controle remoto
    }
    // Delay da janela de silêncio
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}
