#include <csignal>     // kill
#include <cerrno>      // errno, ESRCH, EPERM
#include <cstring>     // strerror
#include <iostream>    // std::cerr, std::cout
#include <unistd.h>    // pid_t
#include <charconv>    // std::from_chars
#include <optional>    // std::optional
#include <string_view> // std::string_view

static std::optional<long> converter_inteiro(std::string_view texto)
{
    long valor{};
    const char *fim_esperado = texto.data() + texto.size();
    auto [fim_conversao, erro] = std::from_chars(texto.data(), fim_esperado, valor);

    if (erro != std::errc{} || fim_conversao != fim_esperado)
    {
        return std::nullopt;
    }

    return valor;
}

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

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "uso: " << argv[0] << " PID SINAL\n";
        return 1;
    }

    std::optional<long> pid_convertido = converter_inteiro(argv[1]);
    if (!pid_convertido)
    {
        std::cerr << "erro: pid inválido: " << argv[1] << "\n";
        return 1;
    }

    if (*pid_convertido <= 0)
    {
        std::cerr << "erro: pid deve ser positivo: " << *pid_convertido << "\n";
        return 1;
    }

    std::optional<long> sinal_convertido = converter_inteiro(argv[2]);
    if (!sinal_convertido)
    {
        std::cerr << "erro: sinal inválido: " << argv[2] << "\n";
        return 1;
    }

    if (*sinal_convertido < 1 || *sinal_convertido > 31)
    {
        std::cerr << "erro: sinal fora da faixa 1..31: " << *sinal_convertido << "\n";
        return 1;
    }

    pid_t pid_destino = static_cast<pid_t>(*pid_convertido);
    int sinal_destino = static_cast<int>(*sinal_convertido);

    if (!executar_kill(pid_destino, 0))
    {
        return 1;
    }

    if (!executar_kill(pid_destino, sinal_destino))
    {
        return 1;
    }

    std::cout << "sinal " << sinal_destino << " enviado para o processo "
              << pid_destino << "\n";

    return 0;
}
