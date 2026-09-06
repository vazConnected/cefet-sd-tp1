#include <cerrno>      // errno, EINTR
#include <charconv>    // std::from_chars
#include <cstdio>      // std::snprintf
#include <cstring>     // strerror
#include <iostream>    // std::cerr, std::cout
#include <optional>    // std::optional
#include <random>      // std::mt19937, std::uniform_int_distribution
#include <string_view> // std::string_view
#include <sys/wait.h>  // waitpid
#include <unistd.h>    // pipe, fork, read, write, close

static const size_t TAMANHO_MENSAGEM = 20;

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

static bool escrever_completo(int descritor, const char *dados, size_t total)
{
    size_t escritos = 0;

    while (escritos < total)
    {
        ssize_t resultado = write(descritor, dados + escritos, total - escritos);

        if (resultado == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }

        escritos += static_cast<size_t>(resultado);
    }

    return true;
}

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

static bool enviar_numero(int escrita, long long numero)
{
    char mensagem[TAMANHO_MENSAGEM + 1];
    std::snprintf(mensagem, sizeof(mensagem), "%020lld", numero);

    if (!escrever_completo(escrita, mensagem, TAMANHO_MENSAGEM))
    {
        std::cerr << "erro: falha ao escrever no pipe: " << strerror(errno) << "\n";
        return false;
    }

    return true;
}

static bool executar_produtor(int escrita, long long quantidade)
{
    std::mt19937 gerador{std::random_device{}()};
    std::uniform_int_distribution<int> incremento{1, 100};

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
}

static bool executar_consumidor(int leitura)
{
    char mensagem[TAMANHO_MENSAGEM];
    long long processados = 0;
    long long primos = 0;

    while (true)
    {
        ssize_t lidos = ler_completo(leitura, mensagem, TAMANHO_MENSAGEM);

        if (lidos == -1)
        {
            std::cerr << "erro: falha ao ler do pipe: " << strerror(errno) << "\n";
            return false;
        }

        if (lidos == 0)
        {
            std::cerr << "erro: pipe fechado antes do numero de termino\n";
            return false;
        }

        if (static_cast<size_t>(lidos) < TAMANHO_MENSAGEM)
        {
            std::cerr << "erro: mensagem incompleta no pipe: " << lidos << " bytes\n";
            return false;
        }

        long long numero{};
        auto [fim_conversao, erro] =
            std::from_chars(mensagem, mensagem + TAMANHO_MENSAGEM, numero);

        if (erro != std::errc{} || fim_conversao != mensagem + TAMANHO_MENSAGEM)
        {
            std::cerr << "erro: mensagem invalida no pipe\n";
            return false;
        }

        if (numero == 0)
        {
            break;
        }

        processados += 1;

        if (eh_primo(numero))
        {
            primos += 1;
            std::cout << numero << " é primo\n";
        }
        else
        {
            std::cout << numero << " não é primo\n";
        }
    }

    std::cout << "consumidor encerrado: " << processados << " números processados, "
              << primos << " primos\n";

    return true;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "uso: " << argv[0] << " QUANTIDADE\n";
        return 1;
    }

    std::optional<long long> quantidade = converter_inteiro(argv[1]);

    if (!quantidade)
    {
        std::cerr << "erro: quantidade inválida: " << argv[1] << "\n";
        return 1;
    }

    if (*quantidade <= 0)
    {
        std::cerr << "erro: quantidade deve ser positiva: " << *quantidade << "\n";
        return 1;
    }

    int descritores[2];

    if (pipe(descritores) == -1)
    {
        std::cerr << "erro: pipe falhou: " << strerror(errno) << "\n";
        return 1;
    }

    std::cout.flush();

    pid_t filho = fork();

    if (filho == -1)
    {
        std::cerr << "erro: fork falhou: " << strerror(errno) << "\n";
        close(descritores[0]);
        close(descritores[1]);
        return 1;
    }

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

    return 0;
}
