#include <atomic>
#include <charconv>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <semaphore.h>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

static std::vector<long long> memoria_compartilhada;
static size_t indice_entrada = 0;
static size_t indice_saida = 0;
static size_t ocupacao_atual = 0;

static sem_t vagas;
static sem_t itens;
static sem_t exclusao;

static std::atomic<long long> reservados_producao{0};
static std::atomic<long long> reservados_consumo{0};
static std::atomic<long long> primos_encontrados{0};

static std::vector<int> historico_ocupacao;
static bool registrar_ocupacao = false;
static bool imprimir_resultado = true;
static long long total_numeros = 100000;

static std::optional<long long> converter_inteiro(std::string_view texto)
{
    long long valor{};
    const char *fim_esperado = texto.data() + texto.size();
    auto [fim_conversao, erro] = std::from_chars(texto.data(), fim_esperado, valor);

    if (erro != std::errc{} || fim_conversao != fim_esperado)
    {
        return std::nullopt;
    }

    return valor;
}

static bool eh_primo(long long numero)
{
    if (numero < 2)
    {
        return false;
    }

    if (numero % 2 == 0)
    {
        return numero == 2;
    }

    for (long long divisor = 3; divisor * divisor <= numero; divisor += 2)
    {
        if (numero % divisor == 0)
        {
            return false;
        }
    }

    return true;
}

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

        if (primo)
        {
            primos_encontrados.fetch_add(1);
        }

        if (imprimir_resultado)
        {
            std::string linha = std::to_string(numero);
            linha += primo ? " é primo\n" : " não é primo\n";
            std::cout << linha;
        }
    }
}

static void imprimir_uso(const char *programa)
{
    std::cerr << "uso: " << programa << " N NP NC [M] [--silencioso] "
              << "[--ocupacao ARQUIVO]\n"
              << "  N   posições da memória compartilhada\n"
              << "  NP  threads produtoras\n"
              << "  NC  threads consumidoras\n"
              << "  M   números a consumir, padrão 100000\n";
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        imprimir_uso(argv[0]);
        return 1;
    }

    std::optional<long long> tamanho = converter_inteiro(argv[1]);
    std::optional<long long> quantidade_produtores = converter_inteiro(argv[2]);
    std::optional<long long> quantidade_consumidores = converter_inteiro(argv[3]);

    if (!tamanho || !quantidade_produtores || !quantidade_consumidores)
    {
        std::cerr << "erro: N, NP e NC devem ser números inteiros\n";
        return 1;
    }

    if (*tamanho < 1 || *quantidade_produtores < 1 || *quantidade_consumidores < 1)
    {
        std::cerr << "erro: N, NP e NC devem ser positivos\n";
        return 1;
    }

    int proximo = 4;

    if (argc > 4 && argv[4][0] != '-')
    {
        std::optional<long long> informado = converter_inteiro(argv[4]);

        if (!informado || *informado < 1)
        {
            std::cerr << "erro: M deve ser um inteiro positivo: " << argv[4] << "\n";
            return 1;
        }

        total_numeros = *informado;
        proximo = 5;
    }

    std::string arquivo_ocupacao;

    for (int i = proximo; i < argc; ++i)
    {
        std::string_view opcao{argv[i]};

        if (opcao == "--silencioso")
        {
            imprimir_resultado = false;
        }
        else if (opcao == "--ocupacao")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "erro: --ocupacao exige o nome do arquivo\n";
                return 1;
            }

            arquivo_ocupacao = argv[i + 1];
            registrar_ocupacao = true;
            i += 1;
        }
        else
        {
            std::cerr << "erro: opção desconhecida: " << argv[i] << "\n";
            return 1;
        }
    }

    memoria_compartilhada.resize(static_cast<size_t>(*tamanho));

    if (registrar_ocupacao)
    {
        historico_ocupacao.reserve(static_cast<size_t>(2 * total_numeros));
    }

    if (sem_init(&vagas, 0, static_cast<unsigned>(*tamanho)) == -1 ||
        sem_init(&itens, 0, 0) == -1 ||
        sem_init(&exclusao, 0, 1) == -1)
    {
        std::cerr << "erro: sem_init falhou: " << strerror(errno) << "\n";
        return 1;
    }

    std::random_device fonte_de_entropia;
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(*quantidade_produtores + *quantidade_consumidores));

    auto inicio = std::chrono::steady_clock::now();

    for (long long i = 0; i < *quantidade_produtores; ++i)
    {
        threads.emplace_back(produtor, fonte_de_entropia());
    }

    for (long long i = 0; i < *quantidade_consumidores; ++i)
    {
        threads.emplace_back(consumidor);
    }

    for (std::thread &thread : threads)
    {
        thread.join();
    }

    auto fim = std::chrono::steady_clock::now();
    double duracao_ms =
        std::chrono::duration<double, std::milli>(fim - inicio).count();

    sem_destroy(&vagas);
    sem_destroy(&itens);
    sem_destroy(&exclusao);

    if (registrar_ocupacao)
    {
        std::ofstream saida{arquivo_ocupacao};

        if (!saida)
        {
            std::cerr << "erro: nao foi possivel escrever em " << arquivo_ocupacao << "\n";
            return 1;
        }

        for (int valor : historico_ocupacao)
        {
            saida << valor << "\n";
        }
    }

    std::cout << "resumo:"
              << " n=" << *tamanho
              << " np=" << *quantidade_produtores
              << " nc=" << *quantidade_consumidores
              << " numeros=" << total_numeros
              << " primos=" << primos_encontrados.load()
              << " tempo_ms=" << duracao_ms
              << "\n";

    return 0;
}
