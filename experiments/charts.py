#!/usr/bin/env python3
"""Gera os graficos do estudo de caso em SVG, sem dependencias externas.

Entrada: results/tempos.csv e results/ocupacao_N_NP_NC.txt, produzidos por
experiments/run_case_study.sh. Saida: results/medias.csv, results/tempo_medio.svg e
results/ocupacao_N_NP_NC.svg.
"""

import csv
import glob
import math
import os
import re
from collections import defaultdict

RESULTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results")
COMBINACOES = [(1, 1), (1, 2), (1, 4), (1, 8), (2, 1), (4, 1), (8, 1)]
CORES = ["#1f77b4", "#d62728", "#2ca02c", "#9467bd"]

LARGURA, ALTURA = 800, 480
MARGEM = {"esq": 80, "dir": 160, "topo": 40, "base": 70}


def svg_inicio():
    return [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{LARGURA}" height="{ALTURA}" '
        f'font-family="sans-serif" font-size="13">',
        f'<rect width="{LARGURA}" height="{ALTURA}" fill="white"/>',
    ]


def eixos(titulo, rotulo_x, rotulo_y, y_max, ticks_x):
    x0, x1 = MARGEM["esq"], LARGURA - MARGEM["dir"]
    y0, y1 = ALTURA - MARGEM["base"], MARGEM["topo"]
    partes = [
        f'<text x="{LARGURA / 2}" y="22" text-anchor="middle" font-size="15">{titulo}</text>'
    ]
    partes.append(f'<line x1="{x0}" y1="{y0}" x2="{x1}" y2="{y0}" stroke="black"/>')
    partes.append(f'<line x1="{x0}" y1="{y0}" x2="{x0}" y2="{y1}" stroke="black"/>')
    divisoes = 5 if y_max >= 5 else int(y_max)
    for i in range(divisoes + 1):
        valor = y_max * i / divisoes
        y = y0 - (y0 - y1) * i / divisoes
        partes.append(
            f'<line x1="{x0}" y1="{y:.1f}" x2="{x1}" y2="{y:.1f}" stroke="#ddd"/>'
        )
        partes.append(
            f'<text x="{x0 - 8}" y="{y + 4:.1f}" text-anchor="end">{valor:g}</text>'
        )
    for posicao, texto in ticks_x:
        x = x0 + (x1 - x0) * posicao
        partes.append(
            f'<line x1="{x:.1f}" y1="{y0}" x2="{x:.1f}" y2="{y0 + 5}" stroke="black"/>'
        )
        partes.append(
            f'<text x="{x:.1f}" y="{y0 + 20}" text-anchor="middle">{texto}</text>'
        )
    partes.append(
        f'<text x="{(x0 + x1) / 2}" y="{ALTURA - 20}" text-anchor="middle">{rotulo_x}</text>'
    )
    partes.append(
        f'<text x="20" y="{(y0 + y1) / 2}" text-anchor="middle" '
        f'transform="rotate(-90 20 {(y0 + y1) / 2})">{rotulo_y}</text>'
    )
    return partes


def ponto(fx, fy, y_max):
    x0, x1 = MARGEM["esq"], LARGURA - MARGEM["dir"]
    y0, y1 = ALTURA - MARGEM["base"], MARGEM["topo"]
    return x0 + (x1 - x0) * fx, y0 - (y0 - y1) * (fy / y_max if y_max else 0)


def grafico_tempos():
    tempos = defaultdict(list)
    with open(os.path.join(RESULTS, "tempos.csv")) as arquivo:
        for linha in csv.DictReader(arquivo):
            chave = (int(linha["n"]), int(linha["np"]), int(linha["nc"]))
            tempos[chave].append(float(linha["tempo_ms"]))

    medias = {chave: sum(v) / len(v) for chave, v in tempos.items()}
    tamanhos = sorted({chave[0] for chave in medias})

    with open(os.path.join(RESULTS, "medias.csv"), "w") as arquivo:
        arquivo.write("n,np,nc,execucoes,tempo_medio_ms,tempo_min_ms,tempo_max_ms\n")
        for chave in sorted(medias):
            v = tempos[chave]
            arquivo.write(
                f"{chave[0]},{chave[1]},{chave[2]},{len(v)},"
                f"{medias[chave]:.3f},{min(v):.3f},{max(v):.3f}\n"
            )

    y_max = math.ceil(max(medias.values()) / 250) * 250
    ticks = [
        (i / (len(COMBINACOES) - 1), f"{np}/{nc}")
        for i, (np, nc) in enumerate(COMBINACOES)
    ]
    svg = svg_inicio() + eixos(
        "Tempo médio de execução (M = 100000, 10 execuções)",
        "threads produtoras/consumidoras",
        "tempo médio (ms)",
        y_max,
        ticks,
    )

    for indice, n in enumerate(tamanhos):
        cor = CORES[indice % len(CORES)]
        pontos = [
            ponto(i / (len(COMBINACOES) - 1), medias[(n, np, nc)], y_max)
            for i, (np, nc) in enumerate(COMBINACOES)
        ]
        caminho = " ".join(f"{x:.1f},{y:.1f}" for x, y in pontos)
        svg.append(
            f'<polyline points="{caminho}" fill="none" stroke="{cor}" stroke-width="2"/>'
        )
        for x, y in pontos:
            svg.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3" fill="{cor}"/>')
        ly = MARGEM["topo"] + 20 * indice
        lx = LARGURA - MARGEM["dir"] + 20
        svg.append(
            f'<line x1="{lx}" y1="{ly}" x2="{lx + 25}" y2="{ly}" stroke="{cor}" stroke-width="2"/>'
        )
        svg.append(f'<text x="{lx + 32}" y="{ly + 4}">N = {n}</text>')

    svg.append("</svg>")
    with open(os.path.join(RESULTS, "tempo_medio.svg"), "w") as arquivo:
        arquivo.write("\n".join(svg))


def grafico_ocupacao(caminho):
    n, np, nc = map(
        int, re.search(r"ocupacao_(\d+)_(\d+)_(\d+)\.txt$", caminho).groups()
    )
    with open(caminho) as arquivo:
        valores = [int(x) for x in arquivo]

    faixas = 1000
    tamanho = max(1, len(valores) // faixas)
    minimos, maximos, medias = [], [], []
    for inicio in range(0, len(valores), tamanho):
        bloco = valores[inicio : inicio + tamanho]
        minimos.append(min(bloco))
        maximos.append(max(bloco))
        medias.append(sum(bloco) / len(bloco))

    y_max = n
    ticks = [(f, f"{int(len(valores) * f)}") for f in (0, 0.25, 0.5, 0.75, 1)]
    svg = svg_inicio() + eixos(
        f"Ocupação do buffer: N = {n}, NP = {np}, NC = {nc}",
        "operações (produção ou consumo)",
        "posições ocupadas",
        y_max,
        ticks,
    )

    total = len(medias)
    superior = [ponto(i / (total - 1), v, y_max) for i, v in enumerate(maximos)]
    inferior = [
        ponto(i / (total - 1), v, y_max) for i, v in reversed(list(enumerate(minimos)))
    ]
    banda = " ".join(f"{x:.1f},{y:.1f}" for x, y in superior + inferior)
    svg.append(
        f'<polygon points="{banda}" fill="#1f77b4" fill-opacity="0.25" stroke="none"/>'
    )
    linha = " ".join(
        f"{x:.1f},{y:.1f}"
        for x, y in (ponto(i / (total - 1), v, y_max) for i, v in enumerate(medias))
    )
    svg.append(
        f'<polyline points="{linha}" fill="none" stroke="#1f77b4" stroke-width="1.5"/>'
    )

    lx, ly = LARGURA - MARGEM["dir"] + 20, MARGEM["topo"]
    svg.append(
        f'<rect x="{lx}" y="{ly - 6}" width="25" height="12" fill="#1f77b4" fill-opacity="0.25"/>'
    )
    svg.append(f'<text x="{lx + 32}" y="{ly + 4}">mín/máx</text>')
    svg.append(
        f'<line x1="{lx}" y1="{ly + 20}" x2="{lx + 25}" y2="{ly + 20}" stroke="#1f77b4" stroke-width="2"/>'
    )
    svg.append(f'<text x="{lx + 32}" y="{ly + 24}">média</text>')
    svg.append(
        f'<text x="{lx}" y="{ly + 48}" font-size="11">{tamanho} operações por ponto</text>'
    )

    svg.append("</svg>")
    with open(caminho.replace(".txt", ".svg"), "w") as arquivo:
        arquivo.write("\n".join(svg))


if __name__ == "__main__":
    grafico_tempos()
    for caminho in sorted(glob.glob(os.path.join(RESULTS, "ocupacao_*.txt"))):
        grafico_ocupacao(caminho)
    print("gráficos gerados em", os.path.normpath(RESULTS))
