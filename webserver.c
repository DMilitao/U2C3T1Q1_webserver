

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h" // Para acessar netif_default e IP

// Configurações de Wi-Fi
#define WIFI_SSID "ALCL-9D58"
#define WIFI_PASSWORD "xxxxxxxxxx"

// Definição dos pinos dos LEDs
#define LED_PIN CYW43_WL_GPIO_LED_PIN
#define BTN_A 5 // Pino para btn A
#define JS_x 27 // Pino para eixo do Joystick
#define CHAN_JS_x 1 // ADC channel correspondente JS_x

uint16_t adc_x_raw = 0;
bool btn_a_state = 0;

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    tcp_recved(tpcb, p->tot_len);
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    if (strstr(request, "GET / ") == NULL) { // Verifica que a request é válida (não tenho subpágina, apenas fica constantemente atualizando a página principal)
        free(request);
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_OK;
    }

    printf("Request: %s\n", request);

    // Cria a resposta HTML
    char html[4096];

    snprintf(html, sizeof(html),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<title>Pico W Monitor</title>\n"
        "<meta http-equiv=\"refresh\" content=\"1\">\n" // Força a página a recarregar a cada 1s para atualizar dados
        "<style>\n"
            "body { background-color: #2c3e50; font-family: sans-serif; color: #ecf0f1; }\n"
            ".card { background-color: #34495e; max-width: 400px; margin: 50px auto; padding: 20px; border-radius: 8px; }\n"
            "h1 { text-align: center; color: #3498db; }\n"
            "p { font-size: 1.2em; }\n"
            ".value { font-weight: bold; color: #ffffff; float: right; }\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "<div class=\"card\">\n"
            "<h1>Monitor de Sensores</h1>\n"
            "<p>Botao Esquerdo: <span class=\"value\">%d</span></p>\n"
            "<p>Joystick (Eixo X): <span class=\"value\">%d</span></p>\n"
        "</div>\n"
        "</body>\n"
        "</html>\n",
        btn_a_state,
        adc_x_raw );
        // Envia a informação para a página
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY); 
    tcp_output(tpcb);
        // Limpa o buffer e fecha o link com a página
    free(request);
    pbuf_free(p);
    tcp_close(tpcb); 
    
    return ERR_OK;
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Função principal
int main()
{
    stdio_init_all();

    while (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    cyw43_arch_gpio_put(LED_PIN, 0);
    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
    }

    printf("Conectado ao Wi-Fi\n");

    if (netif_default)
    {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Configura o servidor TCP
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    server = tcp_listen(server);
    tcp_accept(server, tcp_server_accept);

    printf("Servidor ouvindo na porta 80\n");

    // Inicializa ADC e configura para JS_x
    adc_init();
    adc_gpio_init(JS_x);

    // Inicializa BTN_A
    gpio_init(BTN_A);
    gpio_set_dir(BTN_A, GPIO_IN); // input
    gpio_pull_up(BTN_A); // habilita pull-up para BTN_A

    while (true)
    {
        cyw43_arch_poll();
            // Lê ADC
        adc_select_input(CHAN_JS_x);
        adc_x_raw = adc_read();

            // Lê botão
        btn_a_state = !gpio_get(BTN_A); 

        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));

    }

    cyw43_arch_deinit();
    return 0;
} // end main
