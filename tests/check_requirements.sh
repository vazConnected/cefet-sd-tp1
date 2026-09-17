#!/usr/bin/env bash

cd "$(dirname "$0")/.." || exit 1
tmp=$(mktemp -d)
erros=0

make -s || exit 1

echo "### Parte 2 - sinais"

# argumentos invalidos tem que dar erro
./bin/sender abc 10  2>/dev/null && { echo "erro: aceitou pid nao numerico"; erros=$((erros+1)); }
./bin/sender 1 0     2>/dev/null && { echo "erro: aceitou sinal 0"; erros=$((erros+1)); }
./bin/sender 1 40    2>/dev/null && { echo "erro: aceitou sinal 40"; erros=$((erros+1)); }
./bin/sender 999999 10 2>/dev/null && { echo "erro: aceitou pid inexistente"; erros=$((erros+1)); }
./bin/sender 1 10    2>/dev/null && { echo "erro: enviou sinal para o init"; erros=$((erros+1)); }
./bin/receiver xyz   2>/dev/null && { echo "erro: receiver aceitou modo invalido"; erros=$((erros+1)); }

for modo in block busy; do
    ./bin/receiver $modo > $tmp/$modo.txt &
    pid=$!
    sleep 0.3
    ./bin/sender $pid 10 > /dev/null
    ./bin/sender $pid 12 > /dev/null
    kill -USR1 $pid
    kill -USR2 $pid
    sleep 0.3
    echo "cpu no modo $modo: $(ps -o %cpu= -p $pid)%"
    ./bin/sender $pid 15 > /dev/null
    wait $pid
    if [ $? -ne 0 ]; then
        echo "erro: receiver $modo nao saiu com 0"; erros=$((erros+1))
    fi
    # 2 de cada: um pelo sender e um pelo kill
    u1=$(grep -c SIGUSR1 $tmp/$modo.txt)
    u2=$(grep -c SIGUSR2 $tmp/$modo.txt)
    if [ "$u1" != 2 ] || [ "$u2" != 2 ] || ! grep -q SIGTERM $tmp/$modo.txt; then
        echo "erro: receiver $modo perdeu sinais"; cat $tmp/$modo.txt; erros=$((erros+1))
    fi
done

echo "### Parte 3 - pipes"

./bin/producer_consumer_pipe 0   2>/dev/null && { echo "erro: aceitou quantidade 0"; erros=$((erros+1)); }
./bin/producer_consumer_pipe abc 2>/dev/null && { echo "erro: aceitou quantidade invalida"; erros=$((erros+1)); }

./bin/producer_consumer_pipe 10000 > $tmp/pipe.txt || { echo "erro: pipe retornou $?"; erros=$((erros+1)); }
python3 tests/check_output.py pipe $tmp/pipe.txt || erros=$((erros+1))
sleep 0.2
if pgrep -f bin/producer_consumer_pipe > /dev/null; then
    echo "erro: sobrou processo do pipe"; erros=$((erros+1))
fi

echo "### Parte 4 - semaforos"

./bin/producer_consumer_semaphore 0 1 1       2>/dev/null && { echo "erro: aceitou N=0"; erros=$((erros+1)); }
./bin/producer_consumer_semaphore 1 1 1 5 --x 2>/dev/null && { echo "erro: aceitou opcao invalida"; erros=$((erros+1)); }

./bin/producer_consumer_semaphore 10 2 2 20000 > $tmp/sem.txt || { echo "erro: semaforo retornou $?"; erros=$((erros+1)); }
python3 tests/check_output.py sem $tmp/sem.txt || erros=$((erros+1))

# casos extremos: buffer de 1 posicao e 8 threads de um lado. se travar, o timeout mata.
for cfg in "1 8 1" "1 1 8" "1000 8 1" "1000 1 8" "10 4 4"; do
    set -- $cfg
    if ! timeout 60 ./bin/producer_consumer_semaphore $1 $2 $3 100000 --silencioso --ocupacao $tmp/oc.txt > /dev/null; then
        echo "erro: N=$1 NP=$2 NC=$3 nao terminou"; erros=$((erros+1)); continue
    fi
    # ocupacao: 2M linhas, entre 0 e N, muda de 1 em 1, acaba em 0
    awk -v n=$1 '
        NR > 1 && ($1 - ant) * ($1 - ant) != 1 { print "salto na linha " NR; bad=1 }
        $1 < 0 || $1 > n { print "fora da faixa na linha " NR; bad=1 }
        { ant = $1 }
        END { if (NR != 200000 || ant != 0) { print "tamanho " NR " ultimo " ant; bad=1 }; exit bad }
    ' $tmp/oc.txt || { echo "erro: ocupacao errada em N=$1 NP=$2 NC=$3"; erros=$((erros+1)); }
    echo "N=$1 NP=$2 NC=$3 ok"
done

rm -rf $tmp
echo
if [ $erros -eq 0 ]; then
    echo "tudo passou"
else
    echo "$erros erro(s)"
    exit 1
fi
