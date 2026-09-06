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

Dois programas independentes. O `sender` envia um sinal a qualquer processo, e o
`receiver` captura sinais e reage a cada um deles com uma mensagem distinta.

### Execução

```bash
make                      # compila os binários
./bin/receiver block &    # inicia o receiver em segundo plano
RPID=$!                   # guarda o PID do processo iniciado com &
./bin/sender $RPID 10     # SIGUSR1 recebido
./bin/sender $RPID 12     # SIGUSR2 recebido
kill -USR1 $RPID          # SIGUSR1 recebido, agora pelo comando do sistema
ps -o %cpu= -p $RPID      # uso de processador durante a espera
./bin/sender $RPID 15     # SIGTERM recebido: encerrando o receiver
```

O `&` devolve o prompt ao usuário, pois o receiver permanece em execução até
receber SIGTERM. A variável `$!` contém o identificador do último processo
iniciado em segundo plano, e o receiver também imprime o próprio identificador
ao iniciar. O envio por `kill` verifica que a captura independe do programa
emissor.

A ordem entre a confirmação do `sender` e a mensagem do `receiver` pode variar,
pois são processos distintos escrevendo no mesmo terminal. Ao final, o shell
informa `Done`, e não `Terminated`, o que confirma que o encerramento partiu da
handler do programa e não da ação padrão do sinal.

Repetindo o roteiro com `busy` no lugar de `block`, a sequência de mensagens é a
mesma e apenas a medição do `ps` se altera. Nos testes realizados, a ocupação
foi de 0,0% no modo `block` e de 98,8% no modo `busy`.

### sender

```bash
./bin/sender PID SINAL
```

Valida os dois argumentos, verifica se o processo alvo existe e envia o sinal. A
verificação utiliza o sinal nulo (`kill(pid, 0)`), que não é entregue ao
processo: o kernel executa apenas as checagens de existência e de permissão. São
aceitos os sinais padrão do POSIX, na faixa de 1 a 31. O programa retorna 0 em
caso de sucesso e 1 em qualquer erro.

Limitação conhecida: entre a verificação e o envio há uma condição de corrida
inerente ao mecanismo, pois o processo alvo pode terminar nesse intervalo e o
sistema pode reutilizar o identificador. Nenhuma API de sinais elimina essa
condição. O programa trata o erro nas duas chamadas, o que limita o efeito
prático do problema.

### receiver

```bash
./bin/receiver busy|block
```

São capturados três sinais: SIGUSR1 (10) e SIGUSR2 (12), que apenas relatam o
recebimento, e SIGTERM (15), cuja handler encerra o processo. Os handlers são
instalados com `sigaction`, e não com `signal`, cuja semântica de reinstalação
varia entre sistemas.

Os handlers de SIGUSR1 e SIGUSR2 apenas marcam uma variável do tipo
`volatile sig_atomic_t`, e a impressão ocorre no laço principal, pois
`std::cout` não é async-signal-safe. O handler de SIGTERM precisa encerrar o
processo por exigência do enunciado e, por isso, utiliza `write` e `_exit`, que
são seguras nesse contexto.

Modos de espera:

- `busy`: laço que consulta as variáveis de estado continuamente, sem nenhuma
  chamada bloqueante. Ocupa um núcleo de processador integralmente.
- `block`: bloqueia os três sinais com `sigprocmask` e suspende o processo com
  `sigsuspend`. Não consome processador durante a espera. A escolha por
  `sigsuspend` em vez de `pause` se deve ao fato de a primeira restaurar a
  máscara de sinais e suspender o processo em uma única operação indivisível.
  Com `pause`, um sinal que chegasse entre o teste da variável de estado e a
  chamada seria perdido, e o processo permaneceria suspenso indefinidamente.

### Tratamento de erros

Cada comando exercita um caminho distinto de validação. Todos retornam 1.

```bash
./bin/sender              # uso: ./bin/sender PID SINAL
./bin/sender abc 15       # erro: pid inválido (não numérico)
./bin/sender 12ab 15      # erro: pid inválido (caracteres residuais)
./bin/sender -5 15        # erro: pid deve ser positivo
./bin/sender 1234 15x     # erro: sinal inválido (caracteres residuais)
./bin/sender 1234 99      # erro: sinal fora da faixa 1..31
./bin/sender 999999 15    # erro: processo não existe (ESRCH)
./bin/sender 1 15         # erro: sem permissão (EPERM)
./bin/receiver            # uso: ./bin/receiver busy|block
./bin/receiver turbo      # erro: modo inválido
```

O caso `12ab` é o menos evidente: a conversão de texto para número é
bem-sucedida, pois lê `12` e interrompe no caractere `a`. O erro só é detectável
pela posição final da leitura. Sem essa verificação, o programa enviaria o sinal
ao processo 12.

## Parte 3: Pipes

Programa único que implementa produtor e consumidor em dois processos ligados
por um pipe anônimo.

```bash
./bin/producer_consumer_pipe QUANTIDADE
```

O parâmetro indica quantos números serão gerados. O pipe é criado antes do
`fork`, de modo que os dois processos herdem os mesmos descritores. O processo
pai atua como produtor e o filho como consumidor. Logo após a bifurcação, cada
processo fecha a extremidade que não utiliza. Esse fechamento é necessário para
o funcionamento correto: enquanto qualquer processo mantiver a extremidade de
escrita aberta, o consumidor não recebe indicação de fim de arquivo.

O produtor gera a sequência definida no enunciado, com primeiro termo igual a 1
e incremento aleatório entre 1 e 100. Ao final, envia o número zero e encerra.
O consumidor lê cada número, verifica se é primo, imprime o resultado e termina
ao receber o zero. O processo pai aguarda o filho com `waitpid`, o que evita
processo zumbi, e propaga o resultado do filho em seu próprio código de saída.

Cada número trafega como uma cadeia de exatamente 20 bytes preenchida com zeros
à esquerda. A opção por zeros, e não por espaços, permite que a leitura utilize
a mesma rotina de conversão empregada nos demais programas, pois `std::from_chars`
não ignora espaços iniciais.

As operações de leitura e escrita são repetidas até completar os 20 bytes, pois
`read` e `write` podem transferir menos bytes do que o solicitado, sobretudo
quando o pipe está cheio. Tratar uma transferência parcial como bem-sucedida
desalinharia todas as mensagens seguintes. As duas rotinas também tratam o erro
`EINTR`, retornado quando um sinal interrompe a chamada.

Há uma chamada explícita a `std::cout.flush()` antes do `fork`, pois o buffer de
saída é duplicado junto com o processo. Sem esse descarregamento, qualquer
conteúdo pendente seria impresso duas vezes, uma por processo.

Averiguação dos argumentos:

```bash
./bin/producer_consumer_pipe        # uso: ./bin/producer_consumer_pipe QUANTIDADE
./bin/producer_consumer_pipe abc    # erro: quantidade inválida
./bin/producer_consumer_pipe 0      # erro: quantidade deve ser positiva
```

Averiguação da execução:

```bash
./bin/producer_consumer_pipe 10             # saída legível diretamente
./bin/producer_consumer_pipe 1000 | tail -1 # linha de resumo com os totais
ps -eo stat,comm | grep producer_consumer   # nenhum processo remanescente
```

Nos testes realizados, a execução com mil números produziu mil linhas de
resultado mais a linha de resumo. Verificou-se que a sequência inicia em 1, é
estritamente crescente e apresenta incrementos entre 1 e 100. A classificação
de primalidade foi conferida contra uma implementação independente, sem
divergências. A execução com cem mil números exercita o bloqueio por pipe
cheio, já que a capacidade padrão no Linux é de 64 KB, equivalente a 3.276
mensagens de 20 bytes, e concluiu corretamente.

## Parte 4: Produtor-Consumidor com semáforos

A documentar.
