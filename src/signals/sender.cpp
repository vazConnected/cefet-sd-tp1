#include <csignal>     // kill, SIGTERM
#include <cerrno>      // errno, ESRCH, EPERM
#include <iostream>    // std::cerr, std::cout
#include <unistd.h>    // pid_t
#include <charconv>    // std::from_chars
#include <string_view> // std::string_view

int main(int argc, char *argv[])
{
    // argv[0] é o nome do programa, logo 2 argumentos => argc == 3
    if (argc != 3)
    {
        std::cerr << "uso: " << argv[0] << " <pid> <sinal>\n";
        return 1;
    }

    std::string_view pid_argumento{argv[1]};

    // from_chars escreve o resultado aqui e devolve (fim da leitura, erro)
    long pid_convertido{};
    auto [pid_fim_conversao, pid_erro_conversao] = std::from_chars(
        pid_argumento.data(),
        pid_argumento.data() + pid_argumento.size(),
        pid_convertido);

    // errc{} é o valor "sem erro"; cobre texto não numérico e estouro de faixa
    if (pid_erro_conversao != std::errc{})
    {
        std::cerr << "erro: pid inválido: " << argv[1] << "\n";
        return 1;
    }

    if (pid_fim_conversao != pid_argumento.data() + pid_argumento.size())
    {
        std::cerr << "erro: caracteres inesperados no pid: " << argv[1] << "\n";
        return 1;
    }

    if (pid_convertido <= 0)
    {
        std::cerr << "erro: pid deve ser positivo: " << pid_convertido << "\n";
        return 1;
    }

    // a partir daqui o valor é confiável; kill espera pid_t, não long
    pid_t pid_destino = static_cast<pid_t>(pid_convertido);

    return 0;
}
