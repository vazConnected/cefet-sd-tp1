#include <csignal>     // sigaction, sigsuspend, sigprocmask
#include <cerrno>      // errno
#include <cstring>     // strerror
#include <iostream>    // std::cerr, std::cout
#include <string_view> // std::string_view
#include <unistd.h>    // getpid, write, _exit

static volatile sig_atomic_t recebido_usr1 = 0;
static volatile sig_atomic_t recebido_usr2 = 0;

static void tratar_usr1(int)
{
    recebido_usr1 = 1;
}

static void tratar_usr2(int)
{
    recebido_usr2 = 1;
}

static void tratar_term(int)
{
    const char mensagem[] = "SIGTERM recebido: encerrando o receiver\n";
    ssize_t escritos = write(STDOUT_FILENO, mensagem, sizeof(mensagem) - 1);
    (void)escritos;
    _exit(0);
}

static bool instalar_handler(int sinal, void (*tratador)(int))
{
    struct sigaction acao{};
    acao.sa_handler = tratador;
    sigemptyset(&acao.sa_mask);
    acao.sa_flags = 0;

    if (sigaction(sinal, &acao, nullptr) == -1)
    {
        std::cerr << "erro: sigaction falhou para o sinal " << sinal << ": "
                  << strerror(errno) << "\n";
        return false;
    }

    return true;
}

static void relatar_sinais_recebidos()
{
    if (recebido_usr1)
    {
        recebido_usr1 = 0;
        std::cout << "SIGUSR1 recebido\n";
    }

    if (recebido_usr2)
    {
        recebido_usr2 = 0;
        std::cout << "SIGUSR2 recebido\n";
    }
}

static void esperar_ocupado()
{
    while (true)
    {
        relatar_sinais_recebidos();
    }
}

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

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "uso: " << argv[0] << " <busy|block>\n";
        return 1;
    }

    std::string_view modo{argv[1]};

    if (modo != "busy" && modo != "block")
    {
        std::cerr << "erro: modo inválido: " << argv[1] << " (use busy ou block)\n";
        return 1;
    }

    if (!instalar_handler(SIGUSR1, tratar_usr1) ||
        !instalar_handler(SIGUSR2, tratar_usr2) ||
        !instalar_handler(SIGTERM, tratar_term))
    {
        return 1;
    }

    std::cout << std::unitbuf;
    std::cout << "receiver iniciado no modo " << modo
              << " (pid " << getpid() << ")\n";

    if (modo == "busy")
    {
        esperar_ocupado();
    }
    else if (!esperar_bloqueado())
    {
        return 1;
    }

    return 0;
}
