#!/usr/bin/env bash

set -eu
cd "$(dirname "$0")/.."

M=100000
REPETICOES=10
TAMANHOS="1 10 100 1000"
COMBINACOES="1,1 1,2 1,4 1,8 2,1 4,1 8,1"

make -s
mkdir -p results
echo "n,np,nc,repeticao,tempo_ms" >results/tempos.csv

for n in $TAMANHOS; do
    for combo in $COMBINACOES; do
        np=${combo%,*}
        nc=${combo#*,}
        for rep in $(seq 1 $REPETICOES); do
            resumo=$(./bin/producer_consumer_semaphore "$n" "$np" "$nc" "$M" --silencioso)
            tempo=${resumo##*tempo_ms=}
            echo "$n,$np,$nc,$rep,$tempo" >>results/tempos.csv
        done
        ./bin/producer_consumer_semaphore "$n" "$np" "$nc" "$M" --silencioso \
            --ocupacao "results/ocupacao_${n}_${np}_${nc}.txt" >/dev/null
        echo "n=$n np=$np nc=$nc concluido"
    done
done
