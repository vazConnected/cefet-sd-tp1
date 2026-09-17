#!/usr/bin/env python3
# Confere a saida do consumidor (pipe ou semaforo) linha a linha.
# uso: check_output.py pipe|sem ARQUIVO

import sys


def primo(n):
    if n < 2:
        return False
    if n % 2 == 0:
        return n == 2
    d = 3
    while d * d <= n:
        if n % d == 0:
            return False
        d += 2
    return True


modo, arquivo = sys.argv[1], sys.argv[2]
linhas = open(arquivo).read().splitlines()
resumo = linhas.pop()  # ultima linha e o resumo
qtd_primos = 0
anterior = None

for linha in linhas:
    n = int(linha.split()[0])
    eh = primo(n)
    if ("não" in linha) == eh:
        sys.exit(f"primalidade errada: {linha}")
    qtd_primos += eh

    if modo == "pipe":
        if anterior is None and n != 1:
            sys.exit(f"primeiro termo deveria ser 1, veio {n}")
        if anterior is not None and not 1 <= n - anterior <= 100:
            sys.exit(f"incremento fora de [1,100]: {anterior} -> {n}")
        anterior = n
    else:
        if not 1 <= n <= 10**7:
            sys.exit(f"numero fora da faixa: {n}")

if modo == "pipe":
    esperado = f"{len(linhas)} números processados, {qtd_primos} primos"
else:
    esperado = f"primos={qtd_primos}"

if esperado not in resumo:
    sys.exit(f"resumo nao bate: '{resumo}' (esperado {esperado})")

print(f"{len(linhas)} numeros, {qtd_primos} primos, tudo certo")
