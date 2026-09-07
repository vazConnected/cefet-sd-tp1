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
- `scripts/`: automação dos estudos de caso da Parte 4, ainda vazio
- `bin/`: binários gerados pelo build, não versionado
- `results/`: saídas dos experimentos da Parte 4, não versionado

## Build

```bash
make             # compila todos os binários em bin/
make bin/sender  # compila apenas um binário
make clean       # remove bin/
```

O build produz três binários: `bin/sender` e `bin/receiver`, da Parte 2, e
`bin/producer_consumer_pipe`, da Parte 3. Flags: `-std=c++17 -Wall -Wextra -O2`.
A compilação deve terminar sem nenhum aviso.

O projeto usa um único Makefile na raiz. Cada programa é um único arquivo `.cpp`
compilado com um só comando, e as flags são idênticas entre eles. Arquivos de
build separados duplicariam a configuração e ainda exigiriam um Makefile na raiz
para evitar que o usuário compilasse cada parte individualmente.

## Parte 2: Sinais

Dois programas independentes. O `sender` envia um sinal a qualquer processo, e o
`receiver` captura sinais e reage a cada um deles com uma mensagem distinta.

Um sinal é uma notificação assíncrona entregue pelo kernel a um processo. O
processo que recebe pode definir uma função de tratamento, chamada handler, que
o kernel executa interrompendo o fluxo normal de execução.

### Execução

```bash
make                      # compila os binários
./bin/receiver block &    # inicia o receiver em segundo plano
RPID=$!                   # guarda o PID do processo iniciado com &
./bin/sender $RPID 10     # envia SIGUSR1
./bin/sender $RPID 12     # envia SIGUSR2
kill -USR1 $RPID          # envia SIGUSR1 pelo comando do sistema
ps -o %cpu= -p $RPID      # uso de processador durante a espera
./bin/sender $RPID 15     # envia SIGTERM, que encerra o receiver
```

Saída correspondente:

```
receiver iniciado no modo block (pid 24360)
SIGUSR1 recebido
sinal 10 enviado para o processo 24360
SIGUSR2 recebido
sinal 12 enviado para o processo 24360
SIGUSR1 recebido
 0.0
SIGTERM recebido: encerrando o receiver
sinal 15 enviado para o processo 24360
[1]+  Done                    ./bin/receiver block
```

O `&` devolve o prompt ao usuário, pois o receiver permanece em execução até
receber SIGTERM. A variável `$!` contém o identificador do último processo
iniciado em segundo plano, e o receiver também imprime o próprio identificador
ao iniciar. O envio por `kill` prova que a captura funciona para qualquer
origem, e não apenas para o programa emissor deste trabalho.

A ordem entre a confirmação do `sender` e a mensagem do `receiver` varia entre
execuções, pois são processos distintos escrevendo no mesmo terminal. A última
linha, `Done` em vez de `Terminated`, confirma que o encerramento partiu da
handler do próprio programa, e não da ação padrão do sinal.

Repetindo o roteiro com `busy` no lugar de `block`, a sequência de mensagens é a
mesma e apenas a medição do `ps` se altera. Nas execuções registradas acima, a
ocupação foi de 0,0% no modo `block` e de 100% no modo `busy`.

### sender

```bash
./bin/sender PID SINAL
```

Ambos os parâmetros são números. O primeiro é o identificador do processo alvo,
e o segundo é o número do sinal, na faixa de 1 a 31, que corresponde aos sinais
padrão do POSIX. Sinais de tempo real, de SIGRTMIN a SIGRTMAX, estão fora do
escopo do trabalho e são recusados.

O programa valida os dois argumentos, verifica se o processo alvo existe e envia
o sinal. A verificação utiliza o sinal nulo, `kill(pid, 0)`, que não é entregue
ao processo: o kernel executa apenas as checagens de existência e de permissão,
o que permite distinguir processo inexistente de processo inacessível. Retorna 0
em caso de sucesso e 1 em qualquer erro.

Limitação conhecida: entre a verificação e o envio há uma condição de corrida
inerente ao mecanismo, pois o processo alvo pode terminar nesse intervalo e o
sistema pode reutilizar o identificador. Nenhuma API de sinais elimina essa
condição. O programa trata o erro nas duas chamadas, o que limita o efeito
prático do problema.

### receiver

```bash
./bin/receiver busy|block
```

O parâmetro escolhe a forma de espera. O programa imprime o próprio PID ao
iniciar e permanece em execução até receber SIGTERM.

São capturados três sinais: SIGUSR1, de número 10, e SIGUSR2, de número 12, que
apenas relatam o recebimento, e SIGTERM, de número 15, cuja handler encerra o
processo. SIGUSR1 e SIGUSR2 foram escolhidos por serem reservados pelo POSIX ao
uso da aplicação, de modo que nenhum outro componente do sistema os envia por
conta própria. Os handlers são instalados com `sigaction`, e não com `signal`,
cuja semântica de reinstalação do handler após a entrega varia entre sistemas.

Os handlers de SIGUSR1 e SIGUSR2 apenas marcam uma variável do tipo
`volatile sig_atomic_t`, e a impressão ocorre no laço principal, pois dentro de
um handler só é permitido chamar funções async-signal-safe, e `std::cout` não é
uma delas. O tipo `sig_atomic_t` garante que a escrita não seja interrompida
pela metade, e o qualificador `volatile` impede que o compilador mantenha a
variável em registrador. O handler de SIGTERM precisa encerrar o processo por
exigência do enunciado e, por isso, utiliza `write` e `_exit`, que são seguras
nesse contexto, enquanto `std::exit` não seria, pois executa destrutores e
rotinas de saída.

Modos de espera:

- `busy`: laço que consulta as variáveis de estado continuamente, sem nenhuma
  chamada bloqueante. Ocupa um núcleo de processador integralmente.
- `block`: bloqueia os três sinais com `sigprocmask` e suspende o processo com
  `sigsuspend`. Não consome processador durante a espera. A escolha por
  `sigsuspend` em vez de `pause` se deve ao fato de a primeira restaurar a
  máscara de sinais e suspender o processo em uma única operação indivisível.
  Com `pause`, um sinal que chegasse entre o teste da variável de estado e a
  chamada seria perdido, e o processo permaneceria suspenso indefinidamente.

Limitação conhecida: os sinais padrão do POSIX não são enfileirados. Se um
SIGUSR1 já está pendente e outro chega antes da entrega, os dois colapsam em uma
única entrega. Em um teste com 200 sinais enviados em rajada, o receiver
imprimiu 192 mensagens. O comportamento é inerente ao mecanismo, pois sinais
indicam que um evento ocorreu, não quantas vezes ocorreu.

## Parte 3: Pipes

Programa único que implementa produtor e consumidor em dois processos ligados
por um pipe anônimo. O parâmetro indica quantos números serão gerados.

### Execução

```bash
make                                        # compila os binários
./bin/producer_consumer_pipe 5              # saída legível diretamente
./bin/producer_consumer_pipe 1000 | tail -1 # apenas a linha de resumo
ps -eo stat,comm | grep producer_consumer   # nenhum processo remanescente
```

Saída de uma execução com cinco números:

```
1 não é primo
85 não é primo
99 não é primo
131 é primo
204 não é primo
consumidor encerrado: 5 números processados, 1 primos
```

Os valores mudam a cada execução, pois o incremento é sorteado. O que se mantém
é o primeiro termo igual a 1, o crescimento estrito da sequência e a linha final
de resumo, emitida pelo consumidor ao receber o número zero. O último comando do
roteiro não deve listar nada, o que confirma que nenhum processo ficou para trás
e valida o uso de `waitpid` pelo processo pai. O programa retorna 0 quando
produtor e consumidor terminam sem erro, e 1 caso contrário.

### Funcionamento

Um pipe é um buffer mantido pelo kernel, com duas extremidades. O que for
escrito em uma delas sai pela outra na mesma ordem, como uma fila. Cada
extremidade é identificada por um descritor de arquivo, o mesmo tipo de
identificador usado para arquivos abertos, e por isso as operações são `read` e
`write`. Duas propriedades do pipe fazem a coordenação acontecer sem nenhum
acordo explícito entre os processos: se o pipe está cheio e um processo tenta
escrever, o kernel o suspende até abrir espaço; se está vazio e um processo
tenta ler, o kernel o suspende até chegar dado.

O programa valida o argumento e chama `pipe`, que preenche um vetor de dois
inteiros: a posição 0 recebe a extremidade de leitura e a posição 1 a de
escrita. Em seguida chama `fork`, que copia o processo inteiro, incluindo
memória, variáveis e a tabela de descritores. A partir dessa linha existem dois
processos executando o mesmo código, e a única diferença é o valor retornado: o
filho recebe 0 e o pai recebe o PID do filho. É esse valor que decide os papéis,
com o pai atuando como produtor e o filho como consumidor.

Como a tabela de descritores foi duplicada, passam a existir quatro referências
ao pipe, e cada processo fecha a extremidade que não utiliza. Esse fechamento
não é higiene, é condição de término: o kernel só sinaliza fim de arquivo ao
leitor quando todas as referências à extremidade de escrita estiverem fechadas.
Se o consumidor mantivesse aberta a extremidade de escrita, ele esperaria
indefinidamente por um dado que ele mesmo poderia enviar.

O produtor gera a sequência definida no enunciado, com primeiro termo igual a 1
e incremento sorteado entre 1 e 100, e escreve cada número no pipe. Ao completar
a quantidade pedida, envia o número zero, que é a marca de término combinada, e
encerra. O consumidor lê um número por vez, verifica se é primo, imprime o
resultado e para ao ler o zero. Por fim, o pai fecha a extremidade de escrita e
chama `waitpid`, o que evita processo zumbi e permite propagar o resultado do
filho em seu próprio código de saída.


Cada número trafega como uma cadeia de exatamente 20 bytes preenchida com zeros
à esquerda. O tamanho fixo é necessário porque o pipe transporta uma sequência
de bytes sem qualquer noção de onde uma mensagem termina e a próxima começa. Se
o produtor escrevesse `1`, depois `85` e depois `99`, o consumidor receberia
`18599` e não teria como separar os três números. Com tamanho fixo, ler 20 bytes
equivale a ler exatamente uma mensagem. O preenchimento usa zeros, e não
espaços, porque `std::from_chars` não ignora espaços iniciais, e assim a leitura
reaproveita a mesma rotina de conversão empregada nos demais programas.

As operações de leitura e escrita são repetidas até completar os 20 bytes, pois
`read` e `write` podem transferir menos bytes do que o solicitado, sobretudo
quando o pipe está cheio. Tratar uma transferência parcial como bem-sucedida
desalinharia todas as mensagens seguintes, pois a leitura seguinte pegaria o
final de uma mensagem e o começo da outra. As duas rotinas também tratam o erro
`EINTR`, retornado quando um sinal interrompe a chamada.

Há uma chamada explícita a `std::cout.flush()` antes do `fork`, pois o buffer de
saída é duplicado junto com o processo. Sem esse descarregamento, qualquer
conteúdo pendente seria impresso duas vezes, uma por processo.

Quando o pipe enche, o produtor bloqueia até que o consumidor libere espaço. A
capacidade medida nesta máquina foi de 65536 bytes, equivalente a 3276
mensagens. O bloqueio foi comprovado suspendendo o consumidor com SIGSTOP e
consultando `/proc/PID/wchan` do produtor, que passou a indicar
`anon_pipe_write`.

## Parte 4: Produtor-Consumidor com semáforos

A implementar.
