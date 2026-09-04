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

Em desenvolvimento.

## Parte 3: Pipes

A documentar.

## Parte 4: Produtor-Consumidor com semáforos

A documentar.
