#include <NilRTOS.h>
#include <NilSerial.h>
#define UNSIGNED_LONG_LIMIT 4294967295
#define Serial NilSerial

int limite = 3000; // tempo limite de acesso à região crítica
int tempo_resposta = 950;
 
int bomba1 = 2; float volume1 = 250.0, periodo1 = 60000.0, velocidade1 = 26.8; // pino que controla o relé da bomba, volume necessário em ml, período e velocidade de enchimento em ml/s
int bomba2 = 8; float volume2 = 100.0, periodo2 = 60000.0, velocidade2 = 9.2;
int fornecimento = 12; // led indicador de acesso ao fornecimento de água (reg crítica)
int resposta = 13;


SEMAPHORE_DECL(sem, 1);
 
void REGIAO_CRITICA(float volume, float* controle, int bomba)
{
  digitalWrite(bomba, LOW); // Liga a bomba
  digitalWrite(fornecimento, HIGH); // Liga o led indicativo de acesso

  digitalWrite(resposta, HIGH); // Sinaliza e espera o tempo de resposta da bomba para começar a computar
  nilThdSleepMilliseconds(tempo_resposta);
  digitalWrite(resposta, LOW);
  
  if (*controle >= limite) // Valor necessário para terminar superior à janela de acesso ao fornecimento
  {    
    nilThdSleepMilliseconds(limite); // Acessa apenas pelo tempo permitido
    *controle -= limite; // Desconta o volume fornecido do volume que ainda falta fornecer e o volume extra
  }
  else // Volume necessário pode ser entregue dentro da janela de acesso
  {
    nilThdSleepMilliseconds(*controle); // Acessa apenas pelo tempo necessário para concluir, descontando-se o volume extra
    *controle -= *controle; // Zera o volume restante (tarefa terminou)
  }

  digitalWrite(fornecimento, LOW); // Desliga o led indicativo de acesso
  digitalWrite(bomba, HIGH); // Desliga a bomba
  
  nilSemSignal(&sem); // Sinaliza o término do acesso
}

NIL_WORKING_AREA(waBomba1, 64);
NIL_THREAD(Bomba1, arg)
{
  volume1 = (volume1 / velocidade1) * 1000.0; // Cálculo do tempo necessário para fornecer todo o volume, convertido para ms
  float controle = volume1; // Controle do volume fornecido
  unsigned long t = millis(); // Obtém o tempo atual
  
  msg_t resposta;
 
  while(TRUE)
  {
    resposta = nilSemWaitTimeoutS(&sem, periodo1); // Espera para acessar a reg crítica
    if (resposta == NIL_MSG_TMO){} // Se a thread falhar ao deadline, ela apenas volta a esperar novamente
    else
      REGIAO_CRITICA(volume1, &controle, bomba1); 

    Serial.println("Bomba 1");
    Serial.print("Volume necessario: "); Serial.println(volume1 * velocidade1);
    Serial.print("Ainda falta fornecer: "); Serial.println(controle * velocidade1);
    Serial.println("");

    if (controle <= 0) // Fornecimento do volume necessário concluído
    {
      controle = volume1; // Reinicia o controle
      if (millis() < t) // Teste de overflow da variável (Ocorre a cada 50 dias mas tinha que tratar, né? ¯\_(?)_/¯)
        nilThdSleepMilliseconds(periodo1 - ((UNSIGNED_LONG_LIMIT - t) + millis())); // Espera pelo restante do período da tarefa (correção da falha do overflow)
      else
        nilThdSleepMilliseconds(periodo1 - (millis() - t)); // Espera pelo restante do período da tarefa (situação normal)
        
      t = millis(); // Obtém o tempo atual (na próxima execução será contabilizada a diferença entre este tempo e o tempo do término da tarefa seguinte, no caso, a duração da tarefa seguinte que será descontada no período durante a espera)
    }
  }
}
 
NIL_WORKING_AREA(waBomba2, 64);
NIL_THREAD(Bomba2, arg) // Similar à thread acima
{
  volume2 = (volume2 / velocidade2) * 1000.0;
  float controle = volume2;
  unsigned long t = millis();
  
  msg_t resposta;
 
  while(TRUE)
  {
    resposta = nilSemWaitTimeoutS(&sem, periodo2);
    if (resposta == NIL_MSG_TMO){}  
    else
      REGIAO_CRITICA(volume2, &controle, bomba2);

    Serial.println("Bomba 2");
    Serial.print("Volume necessario: "); Serial.println(volume2 * velocidade2);
    Serial.print("Ainda falta fornecer: "); Serial.println(controle * velocidade2);
    Serial.println("");
    
    if (controle <= 0)
    {
      controle = volume2;
      if (millis() < t) // overflow
        nilThdSleepMilliseconds(periodo2 - ((UNSIGNED_LONG_LIMIT - t) + millis())); 
      else
        nilThdSleepMilliseconds(periodo2 - (millis() - t)); 
        
      t = millis();
    }
  }
}
 
NIL_THREADS_TABLE_BEGIN() // Definição da tabela de threads
NIL_THREADS_TABLE_ENTRY(NULL, Bomba1, NULL, waBomba1, sizeof(waBomba1))
NIL_THREADS_TABLE_ENTRY(NULL, Bomba2, NULL, waBomba2, sizeof(waBomba2))
NIL_THREADS_TABLE_END()
 
void setup()
{
  pinMode(bomba1, OUTPUT);
  pinMode(bomba2, OUTPUT);
  pinMode(resposta, OUTPUT);

  digitalWrite(bomba1, HIGH); // Desliga a bomba
  digitalWrite(bomba2, HIGH); // Desliga a bomba
  
  Serial.begin(9600);
 
  nilSysBegin();
}
 
void loop()
{
}
