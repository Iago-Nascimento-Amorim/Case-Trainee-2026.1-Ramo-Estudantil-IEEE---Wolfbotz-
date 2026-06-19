# Case Trainee 2026.1 — Ramo Estudantil IEEE · Wolfbotz
Robô autônomo de estacionamento desenvolvido como case técnico do **PCH Trainee 2026**, programa de trainee do Grupo 6 — Wolfbotz, vinculado ao Ramo Estudantil IEEE do CEFET/RJ.

---

## O desafio
O case proposto consistia em projetar e construir um robô com duas funcionalidades: ser controlado remotamente por um controlador, e, de forma completamente autônoma, **detectar uma vaga de estacionamento lateral e executar a manobra de estacionamento** sem intervenção humana. O robô deveria identificar a abertura da vaga enquanto se deslocava, calcular geometricamente se o espaço era suficiente, e então realizar a sequência de movimentos necessária para se posicionar dentro dela.

---

## Hardware
| Componente | Função |
|---|---|
| ESP32 | Microcontrolador principal |
| 2× HC-SR04 | Sensores ultrassônicos de distância (um por lateral) |
| TB6612FNG | Ponte H dupla para controle dos motores |
| 2× Motor N20 | Tração diferencial (esquerda e direita) |
| Bateria LiPo 7.4V | Alimentação do sistema |
| Conversor Step-Down | Regulação para 3.3V (lógica e sensores) |
| Chave switch | Liga/desliga geral do circuito |

---

## Pinagem

### Ponte H → ESP32
| Pino da Ponte H | Pino do ESP32 | Função |
|---|---|---|
| PWMA | D14 | Velocidade — Motor Esquerdo |
| AIN1 | D12 | Direção — Motor Esquerdo |
| AIN2 | D13 | Direção — Motor Esquerdo |
| BIN1 | D25 | Direção — Motor Direito |
| BIN2 | D26 | Direção — Motor Direito |
| PWMB | D27 | Velocidade — Motor Direito |
| VM | 7.4V (direto da bateria via switch) | Alimentação dos motores |
| VCC / STBY | 3.3V (saída do step-down) | Alimentação lógica / chip ativo |

### Sensores HC-SR04 → ESP32
| Sensor | Pino | Pino do ESP32 |
|---|---|---|
| Esquerdo | TRIG | D5 |
| Esquerdo | ECHO | D18 |
| Direito | TRIG | D23 |
| Direito | ECHO | D22 |

Ambos os sensores são alimentados com 3.3V e GND fornecidos pela saída do conversor step-down.

### Motores N20
| Pino da Ponte H | Motor |
|---|---|
| AO1 / AO2 | Motor N20 Esquerdo |
| BO1 / BO2 | Motor N20 Direito |

---

## Arquitetura do software
O firmware foi desenvolvido em C++ para ESP32 utilizando **FreeRTOS** com arquitetura dual-task e arbitragem por semáforo binário.

- **`vTaskBuscaEsquerda`** — monitora o sensor ultrassônico esquerdo continuamente, medindo a distância lateral enquanto o robô avança a velocidade constante. Detecta geometricamente a abertura e o fechamento de uma vaga à esquerda.
- **`vTaskBuscaDireita`** — opera em paralelo com a mesma lógica para o sensor direito, detectando vagas à direita.

Um semáforo binário garante que apenas uma das tasks acesse o hardware de movimento por vez. Quando uma das tasks detecta uma vaga com espaço suficiente (calculado geometricamente com base no tempo de exposição e na velocidade do robô), ela aciona a sequência de manobra automatizada de estacionamento.

---

## Controle remoto
O robô também pode ser operado manualmente via Bluetooth pelo aplicativo **Arduino Bluetooth Controller** (disponível para Android), usando o modo **Gamepad**. Os botões mapeados são:

| Botão no app | Ação |
|---|---|
| FORWARD | Avançar |
| BACK | Recuar |
| LEFT | Girar à esquerda |
| RIGHT | Girar à direita |
| CIRCLE | Iniciar busca autônoma de vaga |
| SQUARE | Parar / cancelar movimento |

---

## Autor
**Iago do Nascimento Amorim**
CEFET/RJ — Ramo Estudantil IEEE - Wolfbotz
