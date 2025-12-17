#include <bits/stdc++.h>
#include <winsock2.h> // Biblioteca de Sockets
#include <windows.h>  // Biblioteca de Sistema (Teclado)
#include <thread>     // Para Multi-threading
#include <mutex>      // Para evitar conflito de teclas
#include <atomic>     // Para variáveis seguras entre threads


using namespace std;

// --- VARIÁVEIS GLOBAIS ---

mutex tecladoMutex;             // Trava para que dois clientes não apertem teclas ao mesmo tempo

atomic<bool> servidorRodando(true); // Controle global para desligar o servidor

SOCKET socketPrincipal;         // Socket principal (global para podermos fechar de qualquer lugar)

// --- FUNÇÃO DE SIMULAÇÃO DE TECLA ---

void pressionarTecla(char comando) {

    // lock_guard protege este bloco: só uma thread entra aqui por vez

    lock_guard<mutex> lock(tecladoMutex);

    INPUT ip;
    ip.type = INPUT_KEYBOARD;
    ip.ki.wScan = 0;
    ip.ki.time = 0;
    ip.ki.dwExtraInfo = 0;

    // Garante que o comando de tecla seja maiúsculo para comparação

    comando = toupper(comando);

    if (comando == 'D') {

        ip.ki.wVk = VK_RIGHT; // Seta Direita

        cout << "[ACAO] Avancando Slide..." << endl;

    } else if (comando == 'E') {

        ip.ki.wVk = VK_LEFT;  // Seta Esquerda

        cout << "[ACAO] Voltando Slide..." << endl;

    } else {

        return; // Ignora outros caracteres

    }

    // 1. Pressiona a tecla

    ip.ki.dwFlags = 0;

    SendInput(1, &ip, sizeof(INPUT));



    // 2. Solta a tecla

    ip.ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(1, &ip, sizeof(INPUT));

}

// --- FUNÇÃO QUE RODA EM CADA THREAD (UM PARA CADA CLIENTE) ---

void tratarCliente(SOCKET clienteSocket) {
    char buffer[4096] = {0}; // Buffer maior para cabecalho HTTP

    while (servidorRodando) {
        int valread = recv(clienteSocket, buffer, 4096, 0);
        if (valread <= 0) break;

        string mensagem(buffer, valread);

        cout << "[DEBUG] Recebido: " << mensagem.substr(0, 50) << "..." << endl; // Mostra o inicio da msg

        // --- LÓGICA DE PARSER HTTP SIMPLES ---
        // O navegador envia "GET /D HTTP/1.1" ou "GET /E HTTP/1.1" ou "GET /LOGOUT HTTP/1.1"

        if (mensagem.find("GET /QUIT") != string::npos) {
            cout << "--- COMANDO DE ENCERRAMENTO GERAL ---" << endl;
            servidorRodando = false;
            closesocket(socketPrincipal);
            break;
        }
        else if (mensagem.find("GET /LOGOUT") != string::npos) {
            cout << "--- Cliente Desconectou ---" << endl;
            // Normalmente enviariamos uma resposta HTTP 200 aqui,
            // mas como é raw socket, apenas fechamos ou ignoramos.
            break;
        }
        else if (mensagem.find("GET /D") != string::npos) {
            pressionarTecla('D');
        }
        else if (mensagem.find("GET /E") != string::npos) {
            pressionarTecla('E');
        }

        // Resposta HTTP Falsa para o navegador nao ficar carregando infinitamente
        // Isso faz o navegador achar que a pagina carregou e liberar o botao de novo
        string resposta = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain\r\n\r\nOK";
        send(clienteSocket, resposta.c_str(), resposta.length(), 0);

        // Em HTTP simples (sem Keep-Alive complexo), geralmente fechamos a conexao apos responder
        // Se quiser manter aberto, remova o break, mas navegadores abrem conexoes novas a cada fetch
        break;
    }
    closesocket(clienteSocket);
}

// --- MAIN (SERVIDOR) ---

int main() {

    WSADATA wsaData;
    struct sockaddr_in servidorAddr, clienteAddr;
    int addrLen = sizeof(clienteAddr);

    // 1. Inicia Winsock

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {

        cout << "Falha no Winsock." << endl;

        return 1;

    }

    // 2. Configura o Socket Principal

    socketPrincipal = socket(AF_INET, SOCK_STREAM, 0);
    servidorAddr.sin_family = AF_INET;
    servidorAddr.sin_addr.s_addr = INADDR_ANY; // Aceita qualquer IP local
    servidorAddr.sin_port = htons(8080);       // Porta 8080

    bind(socketPrincipal, (struct sockaddr *)&servidorAddr, sizeof(servidorAddr));

    // Fila de espera para até 20 conexões simultâneas

    listen(socketPrincipal, 20);

    cout << "============================================" << endl;
    cout << "   SERVIDOR DE SLIDES MULTI-CLIENTE (TCP)   " << endl;
    cout << "============================================" << endl;
    cout << "Status: RODANDO na porta 8080" << endl;
    cout << "Comandos: 'D' (Avancar), 'E' (Voltar)" << endl;
    cout << "Para desligar o servidor remotamente: envie 'QUIT'" << endl;
    cout << "============================================" << endl << endl;



    // Loop Principal: Apenas aceita conexões e delega para threads

    while (servidorRodando) {

        SOCKET clienteSocket = accept(socketPrincipal, (struct sockaddr *)&clienteAddr, &addrLen);

        // Verifica se o servidor foi desligado (pelo comando QUIT)

        if (!servidorRodando) {
            break;
        }

        if (clienteSocket == INVALID_SOCKET) {
             continue; // Se houve erro na conexão, tenta a próxima
        }

        cout << "+++ Novo cliente conectado! Criando thread..." << endl;

        // Cria uma thread independente para cuidar deste cliente

        thread t(tratarCliente, clienteSocket);

        t.detach(); // Deixa a thread rodando em background

    }

    cout << "Servidor encerrado com sucesso." << endl;

    // Limpeza final

    closesocket(socketPrincipal);

    WSACleanup();

    return 0;

}
