# Sistemas Distribuídos - Trabalho Prático 1

Implementação em C++ dos mecanismos de comunicação entre processos propostos no
enunciado (docs/scope.md): sinais, pipes anônimos e produtor-consumidor
multithreaded com semáforos. Disciplina de Sistemas Distribuídos, CEFET-MG.

## Estrutura

- `Makefile`: build único para todos os programas
- `docs/scope.md`: enunciado
- `src/sinais/`: Parte 2, sender e receiver
- `src/pipes/`: Parte 3, produtor-consumidor com pipe
- `src/produtor-consumidor/`: Parte 4, semáforos e threads
- `scripts/`: automação dos estudos de caso
- `bin/`: binários (não versionado)
- `results/`: saídas dos experimentos (não versionado)

## Build

```bash
make             # compila tudo em bin/
make bin/sender  # compila um binário específico
make clean       # remove bin/
```

Flags de compilação: `-std=c++17 -Wall -Wextra -O2`.

O projeto usa um único Makefile na raiz. Cada programa é um único arquivo `.cpp`
compilado com um só comando, e as flags são idênticas entre eles. Arquivos de
build separados duplicariam a configuração e ainda exigiriam um Makefile na raiz
para evitar que o usuário compilasse cada parte individualmente.

## Parte 2: Sinais

### sender

Envia um sinal a um processo indicado por parâmetro.

```bash
./bin/sender <pid> <sinal>
```

O programa valida os dois argumentos, verifica se o processo alvo existe e envia
o sinal. A verificação utiliza o sinal nulo (`kill(pid, 0)`), que não é entregue
ao processo: o kernel executa apenas as checagens de existência e de permissão.
São aceitos os sinais padrão do POSIX, na faixa de 1 a 31. O programa retorna 0
em caso de sucesso e 1 em qualquer erro.

Averiguação. Cada comando exercita um caminho distinto do programa:

```bash
./bin/sender              # uso: ./bin/sender <pid> <sinal>   (argumentos)
./bin/sender abc 15       # erro: pid inválido                (não numérico)
./bin/sender 12ab 15      # erro: pid inválido                (caracteres residuais)
./bin/sender -5 15        # erro: pid deve ser positivo       (valor inválido)
./bin/sender 1234 15x     # erro: sinal inválido              (caracteres residuais)
./bin/sender 1234 99      # erro: sinal fora da faixa 1..31   (faixa)
./bin/sender 999999 15    # erro: processo não existe         (ESRCH)
./bin/sender 1 15         # erro: sem permissão               (EPERM)
```

Todos os casos acima retornam código 1. O caso `12ab` é o menos evidente: a
conversão de texto para número é bem-sucedida, pois lê `12` e interrompe no
caractere `a`. O erro só é detectável pela posição final da leitura. Sem essa
verificação, o programa enviaria o sinal ao processo 12.

Envio efetivo, contra um processo criado para o teste:

```bash
sleep 100 &
PID=$!
./bin/sender $PID 15   # sinal 15 enviado para o processo <PID>
ps -p $PID             # não deve listar nada, pois o processo terminou
```

Limitação conhecida: entre a verificação e o envio há uma condição de corrida
inerente ao mecanismo, pois o processo alvo pode terminar nesse intervalo e o
sistema pode reutilizar o identificador. Nenhuma API de sinais elimina essa
condição. O programa trata o erro nas duas chamadas, o que limita o efeito
prático do problema.

### receiver

Captura sinais e reage a cada um deles com uma mensagem distinta. A forma de
espera é escolhida por parâmetro.

```bash
./bin/receiver <busy|block>
```

São capturados três sinais: SIGUSR1 (10) e SIGUSR2 (12), que apenas relatam o
recebimento, e SIGTERM (15), cuja handler encerra o processo. Os handlers são
instalados com `sigaction`, e não com `signal`, cuja semântica de reinstalação
varia entre sistemas. Ao iniciar, o programa imprime o próprio PID, necessário
para os testes.

Os handlers de SIGUSR1 e SIGUSR2 apenas marcam uma variável do tipo
`volatile sig_atomic_t`. A impressão ocorre no laço principal, pois `std::cout`
não é async-signal-safe. O handler de SIGTERM precisa encerrar o processo por
exigência do enunciado e, por isso, utiliza `write` e `_exit`, que são seguras
nesse contexto.

Modos de espera:

- `busy`: laço que consulta as variáveis de estado continuamente, sem nenhuma
  chamada bloqueante. Ocupa um núcleo de processador integralmente.
- `block`: bloqueia os três sinais com `sigprocmask` e suspende o processo com
  `sigsuspend`. Não consome processador durante a espera. A escolha por
  `sigsuspend` em vez de `pause` se deve ao fato de a primeira restaurar a
  máscara de sinais e suspender o processo em uma única operação indivisível.
  Com `pause`, um sinal que chegasse entre o teste da variável de estado e a
  chamada seria perdido, e o processo permaneceria suspenso indefinidamente.

Averiguação dos argumentos:

```bash
./bin/receiver          # uso: ./bin/receiver <busy|block>
./bin/receiver turbo    # erro: modo inválido
```

Averiguação da captura de sinais e do custo de processamento:

```bash
./bin/receiver block &
RPID=$!
./bin/sender $RPID 10    # SIGUSR1 recebido
./bin/sender $RPID 12    # SIGUSR2 recebido
kill -USR1 $RPID         # SIGUSR1 recebido, agora pelo shell
ps -o %cpu= -p $RPID     # ocupação de processador durante a espera
./bin/sender $RPID 15    # SIGTERM recebido: encerrando o receiver
```

O mesmo roteiro se aplica ao modo `busy`. A sequência de mensagens é idêntica
nos dois casos, e a diferença observável está na medição do `ps`. Nos testes
realizados, a ocupação foi de 0,0% no modo `block` e de 98,8% no modo `busy`.

## Parte 3: Pipes

A documentar.

## Parte 4: Produtor-Consumidor com semáforos

A documentar.
