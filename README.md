# Sistemas Distribuídos - Trabalho Prático 1

## Objetivo

Implementação em C++ dos três mecanismos pedidos em `docs/scope.md`: sinais,
pipes anônimos e produtor-consumidor multithreaded com semáforos. Este
documento descreve como compilar, executar e testar cada programa, e resume as
decisões de implementação.

## Estrutura

- `Makefile`: build de todos os programas
- `docs/scope.md`: enunciado
- `src/sinais/`: Parte 2, `sender.cpp` e `receiver.cpp`
- `src/pipes/`: Parte 3, `producer_consumer_pipe.cpp`
- `src/produtor-consumidor/`: Parte 4, `producer_consumer_semaphore.cpp`
- `tests/`: roteiro de testes (`check_requirements.sh`) e conferidor de saída (`check_output.py`)
- `experiments/`: estudo de caso da Parte 4 (`run_case_study.sh` e `charts.py`)
- `bin/` e `results/`: gerados, não versionados

São quatro programas, cada um em um único arquivo `.cpp`. A Parte 2 tem dois
porque o enunciado pede programas separados para enviar e receber sinais. Todos
validam os argumentos com `std::from_chars`, retornam 0 em caso de sucesso e 1
em caso de erro, com a mensagem na saída de erro.

## Build

Requisitos: Linux, `g++` com C++17, `make` e `python3` para os testes e os
experimentos.

```bash
make          # compila os quatro binários em bin/
make clean    # remove bin/
```

Flags: `-std=c++17 -Wall -Wextra -O2`, mais `-pthread` no programa da Parte 4.
A compilação não emite avisos.

## Testes

```bash
tests/check_requirements.sh
```

O roteiro compila o projeto e exercita as três partes. Só imprime mensagens
começando com `erro:` quando algo falha, e termina com código 1 nesse caso.

- Parte 2: argumentos inválidos, processo inexistente e processo sem permissão
  (PID 1) devem ser recusados. Nos modos `busy` e `block`, o receiver recebe
  SIGUSR1 e SIGUSR2 pelo `sender` e pelo `kill`, encerra com código 0 ao
  receber SIGTERM, e o uso de processador durante a espera é impresso.
- Parte 3: execução com 10000 números. O `check_output.py` refaz o teste de
  primalidade de cada linha, confere que o primeiro termo é 1, que os
  incrementos estão em [1, 100] e que o resumo corresponde à contagem. Depois
  verifica que não sobrou processo.
- Parte 4: execução com 20000 números, com a mesma conferência de primalidade
  e da faixa [1, 10^7]. Em seguida cinco configurações extremas (N = 1 e
  N = 1000 com 8 threads de um lado) rodam sob `timeout` para detectar
  deadlock, e o arquivo de ocupação é conferido com `awk`: 2M linhas, valores
  em [0, N], variação de um em um e zero ao final.

Saída da última execução:

```
### Parte 2 - sinais
cpu no modo block:  0.0%
cpu no modo busy:  101%
### Parte 3 - pipes
10000 numeros, 788 primos, tudo certo
### Parte 4 - semaforos
20000 numeros, 1361 primos, tudo certo
N=1 NP=8 NC=1 ok
N=1 NP=1 NC=8 ok
N=1000 NP=8 NC=1 ok
N=1000 NP=1 NC=8 ok
N=10 NP=4 NC=4 ok

tudo passou
```

## Parte 2: Sinais

### Execução

```bash
./bin/receiver block &    # ou busy
RPID=$!
./bin/sender $RPID 10     # SIGUSR1
./bin/sender $RPID 12     # SIGUSR2
kill -USR1 $RPID          # pelo comando do sistema
ps -o %cpu= -p $RPID      # 0.0 em block, ~100 em busy
./bin/sender 999999 10    # processo inexistente
./bin/sender $RPID 15     # SIGTERM encerra o receiver
```

```
receiver iniciado no modo block (pid 17628)
sinal 10 enviado para o processo 17628
SIGUSR1 recebido
sinal 12 enviado para o processo 17628
SIGUSR2 recebido
SIGUSR1 recebido
 0.0
erro: processo 999999 não existe
sinal 15 enviado para o processo 17628
SIGTERM recebido: encerrando o receiver
[1]+  Done                    ./bin/receiver block
```

A ordem entre as linhas do `sender` e do `receiver` pode variar, pois são
processos distintos. A última linha, `Done` em vez de `Terminated`, mostra que
o encerramento partiu do handler, e não da ação padrão do sinal.

### sender

`./bin/sender PID SINAL`. Aceita sinais de 1 a 31. A existência do processo é
verificada com o sinal nulo, `kill(pid, 0)`, que não é entregue ao processo e
serve apenas para o kernel checar existência e permissão. Isso distingue
`ESRCH` (processo inexistente) de `EPERM` (sem permissão). A mesma função faz o
envio:

```cpp
static bool executar_kill(pid_t pid, int sinal)
{
    if (kill(pid, sinal) == 0)
    {
        return true;
    }

    if (errno == ESRCH)
    {
        std::cerr << "erro: processo " << pid << " não existe\n";
    }
    else if (errno == EPERM)
    {
        std::cerr << "erro: sem permissão para sinalizar o processo " << pid << "\n";
    }
    else
    {
        std::cerr << "erro: kill falhou: " << strerror(errno) << "\n";
    }

    return false;
}
```

Entre a verificação e o envio o processo alvo pode terminar e o PID ser
reutilizado. Essa condição de corrida é inerente à API de sinais.

### receiver

`./bin/receiver busy|block`. Captura SIGUSR1 e SIGUSR2, que apenas imprimem
uma mensagem, e SIGTERM, cujo handler encerra o processo. SIGUSR1 e SIGUSR2
são reservados pelo POSIX para uso da aplicação. Os handlers são instalados com
`sigaction`, cuja semântica é a mesma em todos os sistemas, ao contrário de
`signal`.

Dentro de um handler só podem ser chamadas funções async-signal-safe, e
`std::cout` não é uma delas. Por isso os handlers de SIGUSR1 e SIGUSR2 apenas
marcam uma variável, lida no laço principal. O handler de SIGTERM usa `write` e
`_exit`, que são seguras nesse contexto:

```cpp
static volatile sig_atomic_t recebido_usr1 = 0;

static void tratar_usr1(int)
{
    recebido_usr1 = 1;
}

static void tratar_term(int)
{
    const char mensagem[] = "SIGTERM recebido: encerrando o receiver\n";
    ssize_t escritos = write(STDOUT_FILENO, mensagem, sizeof(mensagem) - 1);
    (void)escritos;
    _exit(0);
}
```

O modo `busy` é um laço que consulta as variáveis sem parar e ocupa um núcleo
inteiro. O modo `block` bloqueia os três sinais com `sigprocmask` e suspende o
processo com `sigsuspend`, que restaura a máscara anterior e adormece em uma
única operação. Com `pause` haveria uma janela entre o teste da variável e a
chamada em que um sinal seria tratado antes da suspensão, e o processo ficaria
suspenso à espera de um sinal já passado.

```cpp
static bool esperar_bloqueado()
{
    sigset_t bloqueados;
    sigemptyset(&bloqueados);
    sigaddset(&bloqueados, SIGUSR1);
    sigaddset(&bloqueados, SIGUSR2);
    sigaddset(&bloqueados, SIGTERM);

    sigset_t anterior;
    if (sigprocmask(SIG_BLOCK, &bloqueados, &anterior) == -1)
    {
        std::cerr << "erro: sigprocmask falhou: " << strerror(errno) << "\n";
        return false;
    }

    while (true)
    {
        while (!recebido_usr1 && !recebido_usr2)
        {
            sigsuspend(&anterior);
        }

        relatar_sinais_recebidos();
    }
}
```

Sinais padrão do POSIX não são enfileirados: dois SIGUSR1 pendentes ao mesmo
tempo resultam em uma única entrega. Em três testes com 200 sinais enviados em
rajada por um laço de `kill`, o receiver imprimiu entre 146 e 166 mensagens.

## Parte 3: Pipes

### Execução

```bash
./bin/producer_consumer_pipe 5
./bin/producer_consumer_pipe 1000 | tail -1
```

```
1 não é primo
85 não é primo
86 não é primo
148 não é primo
157 é primo
consumidor encerrado: 5 números processados, 1 primos
```

Com 1000 números, o segundo comando mostra apenas a linha final:

```
consumidor encerrado: 1000 números processados, 92 primos
```

O primeiro termo é sempre 1 e a sequência é crescente. O consumidor imprime o
resumo ao receber o zero. O roteiro de testes executa o caso com 10000 números
e confere cada linha.

### Funcionamento

O programa cria o pipe, chama `fork` e usa o valor de retorno para definir os
papéis: o filho consome, o pai produz. Cada processo fecha a extremidade que
não usa, o pai espera o filho com `waitpid` e propaga o código de saída.

```cpp
int descritores[2];

if (pipe(descritores) == -1)
{
    std::cerr << "erro: pipe falhou: " << strerror(errno) << "\n";
    return 1;
}

std::cout.flush();

pid_t filho = fork();

if (filho == 0)
{
    close(descritores[1]);
    bool sucesso = executar_consumidor(descritores[0]);
    close(descritores[0]);
    return sucesso ? 0 : 1;
}

close(descritores[0]);
bool sucesso = executar_produtor(descritores[1], *quantidade);
close(descritores[1]);

int situacao = 0;

if (waitpid(filho, &situacao, 0) == -1)
{
    std::cerr << "erro: waitpid falhou: " << strerror(errno) << "\n";
    return 1;
}

if (!sucesso || !WIFEXITED(situacao) || WEXITSTATUS(situacao) != 0)
{
    return 1;
}
```

O `flush` antes do `fork` evita que conteúdo pendente no buffer de saída seja
impresso duas vezes. O fechamento da extremidade de escrita pelo consumidor é
obrigatório: o kernel só sinaliza fim de arquivo ao leitor quando todas as
referências à extremidade de escrita estão fechadas.

O produtor gera a sequência do enunciado e envia zero como marca de término:

```cpp
long long numero = 1;

for (long long i = 0; i < quantidade; ++i)
{
    if (!enviar_numero(escrita, numero))
    {
        return false;
    }

    numero += incremento(gerador);
}

return enviar_numero(escrita, 0);
```

Cada número é enviado como uma cadeia de 20 bytes com zeros à esquerda, gerada
por `snprintf` com o formato `%020lld`. O pipe é um fluxo de bytes sem
delimitação de mensagens, então o tamanho fixo é o que permite ao consumidor
ler exatamente um número por vez. O preenchimento usa zeros porque
`std::from_chars` não ignora espaços iniciais.

Como `read` e `write` podem transferir menos bytes do que o pedido, as duas
operações são repetidas até completar os 20 bytes e tratam `EINTR`:

```cpp
static ssize_t ler_completo(int descritor, char *dados, size_t total)
{
    size_t lidos = 0;

    while (lidos < total)
    {
        ssize_t resultado = read(descritor, dados + lidos, total - lidos);

        if (resultado == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }

        if (resultado == 0)
        {
            break;
        }

        lidos += static_cast<size_t>(resultado);
    }

    return static_cast<ssize_t>(lidos);
}
```

Quando o pipe enche (65536 bytes nesta máquina, ou 3276 mensagens), o produtor
bloqueia até o consumidor liberar espaço. Isso foi verificado suspendendo o
consumidor com SIGSTOP e lendo `/proc/PID/wchan` do produtor, que indicou
`anon_pipe_write`.

## Parte 4: Produtor-Consumidor com semáforos

### Execução

```bash
./bin/producer_consumer_semaphore N NP NC [M] [--silencioso] [--ocupacao ARQUIVO]
./bin/producer_consumer_semaphore 5 1 1 6
./bin/producer_consumer_semaphore 10 2 2 5000 --silencioso --ocupacao results/ocupacao.txt
```

N é o tamanho do vetor, NP e NC são as quantidades de threads produtoras e
consumidoras, e M é o total de números a consumir (padrão 100000).
`--silencioso` suprime a impressão por número e `--ocupacao` grava a ocupação
do buffer após cada operação, um valor por linha.

```
6541546 não é primo
8014130 não é primo
8602575 não é primo
6951223 não é primo
9493157 não é primo
2575664 não é primo
resumo: n=5 np=1 nc=1 numeros=6 primos=0 tempo_ms=0.253236
```

### Funcionamento

O vetor é circular, com índices de entrada e saída. Três semáforos POSIX
coordenam o acesso: `vagas`, inicializado com N, conta posições livres;
`itens`, inicializado com zero, conta posições ocupadas; `exclusao`,
inicializado com um, serializa o acesso ao vetor. Uma produtora bloqueia em
`vagas` quando o buffer está cheio e uma consumidora bloqueia em `itens` quando
está vazio.

```cpp
static void produtor(unsigned semente)
{
    std::mt19937 gerador{semente};
    std::uniform_int_distribution<long long> sorteio{1, 10000000};

    while (reservados_producao.fetch_add(1) < total_numeros)
    {
        long long numero = sorteio(gerador);

        sem_wait(&vagas);
        sem_wait(&exclusao);

        memoria_compartilhada[indice_entrada] = numero;
        indice_entrada = (indice_entrada + 1) % memoria_compartilhada.size();
        ocupacao_atual += 1;

        if (registrar_ocupacao)
        {
            historico_ocupacao.push_back(static_cast<int>(ocupacao_atual));
        }

        sem_post(&exclusao);
        sem_post(&itens);
    }
}
```

```cpp
static void consumidor()
{
    while (reservados_consumo.fetch_add(1) < total_numeros)
    {
        sem_wait(&itens);
        sem_wait(&exclusao);

        long long numero = memoria_compartilhada[indice_saida];
        indice_saida = (indice_saida + 1) % memoria_compartilhada.size();
        ocupacao_atual -= 1;

        if (registrar_ocupacao)
        {
            historico_ocupacao.push_back(static_cast<int>(ocupacao_atual));
        }

        sem_post(&exclusao);
        sem_post(&vagas);

        bool primo = eh_primo(numero);
        ...
    }
}
```

A ordem dos `sem_wait` é obrigatória: primeiro o semáforo contador, depois a
exclusão mútua. Na ordem inversa uma thread adormeceria dentro da região
crítica e nenhuma outra poderia liberar o recurso pelo qual ela espera.

O término usa dois contadores atômicos de reserva. Cada thread reserva um
índice antes de operar e encerra quando o índice atinge M. Assim são produzidos
e consumidos exatamente M números, e nenhuma thread fica bloqueada em um
semáforo ao final.

Cada produtora tem seu próprio `std::mt19937`, para que o gerador não seja um
ponto de contenção adicional. O teste de primalidade e a impressão ocorrem fora
da região crítica. Cada linha é montada em uma `std::string` e escrita em uma
única inserção, para que linhas de threads diferentes não se misturem.

A impressão de cem mil linhas domina o tempo de execução e mascararia o efeito
de N e das threads, por isso os experimentos usam `--silencioso`. O registro de
ocupação é opcional pelo mesmo motivo; quando ativo, o vetor é pré-alocado com
2M posições.

### Estudo de caso

```bash
experiments/run_case_study.sh   # 280 execuções, cerca de 2 minutos
experiments/charts.py           # gera os SVGs em results/
```

O primeiro script executa cada uma das 28 combinações dez vezes com M = 100000
e grava `results/tempos.csv`, mais uma execução por combinação com
`--ocupacao`. O segundo calcula as médias em `results/medias.csv` e gera
`results/tempo_medio.svg`, com uma curva por N, e um
`results/ocupacao_N_NP_NC.svg` por cenário. Os gráficos de ocupação agrupam as
200 mil operações em 1000 faixas e mostram a média e a banda mínimo/máximo de
cada faixa.

Tempo médio em milissegundos, 10 execuções por célula, em máquina com 16
núcleos:

| NP/NC | N = 1 | N = 10 | N = 100 | N = 1000 |
|-------|------:|-------:|--------:|---------:|
| 1/1   | 1110,8 | 111,4 | 81,5 | 78,7 |
| 1/2   | 783,3 | 83,0 | 48,5 | 46,0 |
| 1/4   | 778,6 | 88,8 | 72,8 | 69,4 |
| 1/8   | 788,8 | 110,0 | 113,3 | 113,5 |
| 2/1   | 782,5 | 109,0 | 91,7 | 92,7 |
| 4/1   | 781,1 | 117,2 | 107,3 | 112,6 |
| 8/1   | 763,8 | 128,6 | 129,0 | 134,3 |

Ocupação média do buffer, em percentual de N:

| NP/NC | N = 1 | N = 10 | N = 100 | N = 1000 |
|-------|------:|-------:|--------:|---------:|
| 1/1   | 50,0 | 56,5 | 93,1 | 98,8 |
| 1/2   | 50,0 | 49,0 | 88,3 | 97,8 |
| 1/4   | 50,0 | 37,7 | 6,5 | 2,6 |
| 1/8   | 50,0 | 14,0 | 1,4 | 0,4 |
| 2/1   | 50,0 | 61,8 | 94,7 | 99,0 |
| 4/1   | 50,0 | 67,9 | 96,3 | 99,1 |
| 8/1   | 50,0 | 83,1 | 98,2 | 99,3 |

Observações:

- Com N = 1 o tempo é de seis a dez vezes maior do que com os demais valores.
  Cada operação exige uma troca de contexto entre produtora e consumidora, e a
  ocupação alterna entre 0 e 1. A segunda thread reduz o tempo em cerca de 30%,
  e a partir daí o número de threads não faz diferença, pois com uma única
  posição no máximo uma thread progride por vez.
- N = 10 já elimina a maior parte do custo, e de N = 100 em diante as curvas
  coincidem. O buffer absorve as diferenças de ritmo, e aumentar N além disso
  não traz ganho.
- Para N maior ou igual a 10, a configuração mais rápida é 1/2. A consumidora faz o teste de primalidade e
  é mais lenta que a produtora, o que se vê na ocupação de 93% a 99% nos
  cenários 1/1 com N = 100 e N = 1000: o buffer fica cheio e a produtora
  espera. Duas consumidoras equilibram a carga, e em N = 10 a ocupação cai para
  perto de 50%.
- Com 4 ou 8 consumidoras a única produtora passa a ser o gargalo. Em N = 100 e
  N = 1000 a ocupação fica abaixo de 7%, e em N = 10 cai para 38% e 14%. As
  consumidoras disputam o semáforo `itens` e a exclusão mútua, o que aumenta o
  tempo.
- Com mais produtoras e uma consumidora o tempo cresce com NP, pois a
  consumidora continua sendo o gargalo e as produtoras apenas acrescentam
  contenção em `vagas` e `exclusao`. Em N = 100 e N = 1000 o buffer fica acima
  de 94% de ocupação; em N = 10 a ocupação sobe de 62% para 83% conforme NP
  aumenta.

## Conclusão

Os três programas atendem ao enunciado e passam no roteiro de testes.
Os sinais mostraram comunicação assíncrona e as restrições do código executado
em handlers. Os pipes mostraram comunicação por fluxo de bytes, em que a
delimitação das mensagens e o fechamento dos descritores cabem ao programa. Os
semáforos mostraram que a ordem de aquisição decide a ausência de deadlock e
que o tamanho do buffer e o balanceamento entre produtoras e consumidoras
determinam o desempenho.
