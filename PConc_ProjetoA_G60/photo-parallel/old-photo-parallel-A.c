/******************************************************************************
 * Programacao Concorrente
 * LEEC 24/25
 *
 * Projecto - Parte A
 *                           old-photo-parallel-A.c
 * 
 *  Grupo: 60
 *  Por: Nicollas Nogueira e Gustavo Gonçalves
 * 
 * nº aluno: 110768 e 	110649
 *           
 *****************************************************************************/

#include <gd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <time.h>
#include "image-lib.h"
#include <string.h>
#include <pthread.h>  
#include <unistd.h>
#include <ctype.h>

// Representa informações sobre cada arquivo JPEG (nome e tamanho)
typedef struct {
    char *nome;
    long size; 
} ImageInfo;

// Retorna dados sobre cada thread (ID e tempo de execução)
typedef struct{
    int thread_id;
    struct timespec thread_tempo;
} ThreadResult;

// Armazena dados para serem processados por cada thread (JPEG's, intervalos de processamento e diretórios entrada/saida)
typedef struct {
    ImageInfo **img_arquivos;
    int img_contagem;
    int n_threads;
    int start;           
    int end;                
    gdImagePtr in_texture_img;
    char *diretorio_saida;
    char *diretorio_entrada;
    int thread_id;
} ThreadInfo;

/*
 *Compara os tamanhos de duas struct ImageInfo.

* Retorna a diferença entre os tamanhos das duas imagens.
* Se (tamanho) imagem0 < imagem1, um valor negativo é retornado. imagem0 vem primeiro.
* Se (tamanho) imagem0 > maior que a imagem1, um valor positivo é retornado. imagem1 vem primeiro.
* Se (tamanho) imagem0 = imagem1 , zero é retornado, indicando que os dois elementos são iguais em termos de ordenação.
*/
int compara_tamanho(const void *a, const void *b) {

// Converte os ponteiros void para ponteiros para structs ImageInfo.
    ImageInfo *imagem0 = *(ImageInfo **)a;
    ImageInfo *imagem1 = *(ImageInfo **)b;
    return (imagem0->size - imagem1->size);
}

 /*
 * Função para comparar duas structs ImageInfo pelos seus nomes pelo metódo de ordenação alfabético.
 * 
 * A função `strcasecmp` retorna:
 * ->Um valor negativo se o nome de `imagem0` < `imagem1` na ordem alfabética.
 * ->Zero se os dois nomes forem iguais (ignorando diferenças de maiúsculas/minúsculas).
 * ->Um valor positivo se o nome de `imagem0` > `imagem1` na ordem alfabética.
 *Realiza uma comparação entre cada caracter entre os nomes dos arquivos, para ordenar de forma alfabética.
 */

int compara_nome(const void *a, const void *b) {
    ImageInfo *imagem0 = *(ImageInfo **)a;
    ImageInfo *imagem1 = *(ImageInfo **)b;
    return strcasecmp(imagem0->nome, imagem1->nome);
}

/*
 * process_imgs()
 * 
 * Função utilizada por threads para processar um conjunto de imagens.
 * Realiza operações como leitura de arquivos, aplicação de efeitos nas imagens
 * e gravação das imagens processadas. Além disso, mede o tempo gasto pela thread.
 * 
 * Parâmetros:
 *   - arg: Ponteiro para a struct ThreadInfo, que contém informações
 *          necessárias para o processamento das imagens.
 * Retorno:
 *   - Ponteiro para uma struct ThreadResult com o tempo de execução da thread.
 */

//
void *process_imgs(void *arg) {
    // Cast do ponteiro genérico para a struct ThreadInfo.
    ThreadInfo *data = (ThreadInfo *)arg;

    // mostra o intervalo de imagens processadas por esta thread.
    //printf("Thread %d vai processar imagens de %d para %d.\n", data->thread_id, data->start, data->end);

    struct timespec start_time_thread, end_time_thread;
    // Marca o início da execução da thread usando o CLOCK_MONOTONIC. 
    clock_gettime(CLOCK_MONOTONIC, &start_time_thread);

    // Itera sobre as imagens atribuídas à thread, utilizando um intervalo exclusivo.
    for (int i = data->start; i < data->end; i++) {
        ImageInfo *file = data->img_arquivos[i];

    // Constrói o caminho completo para o arquivo de entrada.
        size_t full_path_size = strlen(data->diretorio_entrada) + strlen(file->nome) + 2;
        char *full_path = malloc(full_path_size);
        if (full_path == NULL) {
            perror("Erro de alocação de memória.");
        }
        snprintf(full_path, full_path_size, "%s/%s", data->diretorio_entrada, file->nome);

    // Constrói o caminho completo para o arquivo de saída.
        size_t out_file_nome_size = strlen(data->diretorio_saida) + strlen(file->nome) + 2; 
        char *out_file_nome = malloc(out_file_nome_size);
        if (out_file_nome == NULL) {
            perror("Erro de alocação de memória.");
        }
        sprintf(out_file_nome, "%s%s", data->diretorio_saida, file->nome);        

    // Verifica se o arquivo de saída já existe para evitar processamento redundante.
        if(access(out_file_nome, F_OK) != -1){
            //Removido para deixar o programa mais rápido. Opção para deixar o programa mais organizado:
            //printf("\nA imagem %s ja foi processada antes . Nao e necessario fazer novamente.\n", out_file_nome);

            continue;
        }

    // Lê o arquivo de entrada e verifica se foi carregado com sucesso. 
        gdImagePtr in_img = read_jpeg_file(full_path);
        if (!in_img) {
            fprintf(stderr, "Erro ao carregar imagem %s\n", file->nome);
            continue;
        }

    // Aplica alterações na imagem usando funções específicas.
        gdImagePtr out_contrast_img = contrast_image(in_img);
        gdImagePtr out_smoothed_img = smooth_image(out_contrast_img);
        gdImagePtr out_textured_img = texture_image(out_smoothed_img, data->in_texture_img);
        gdImagePtr out_sepia_img = sepia_image(out_textured_img);

        
    // Salva a imagem processada no diretório de saída e verifica sucesso.
        if (write_jpeg_file(out_sepia_img, out_file_nome) == 0) {
            fprintf(stderr, "Erro ao salvar imagem %s\n", out_file_nome);
        }

    // Libera os recursos alocados para as imagens.
        gdImageDestroy(out_smoothed_img);
		gdImageDestroy(out_sepia_img);
		gdImageDestroy(out_contrast_img);
		gdImageDestroy(in_img);
    }

// // Aloca memória para armazenar o tempo de execução da thread e seu ID.
    ThreadResult *thread_ret = malloc(sizeof(ThreadResult));
    if (thread_ret == NULL) {
        perror("Erro de alocação de memória.");
        return NULL;
    }

// Marca o fim da execução da thread e calcula o tempo gasto.
    clock_gettime(CLOCK_MONOTONIC, &end_time_thread);
    struct timespec thread_tempo = diff_timespec(&end_time_thread, &start_time_thread); 
    thread_ret->thread_id = data->thread_id;
    thread_ret->thread_tempo = thread_tempo;

// Retorna a struct contendo os resultados da thread.
    return (void *) thread_ret;
}

/* 
 * A função verifica se a string fornecida é um número inteiro válido.
 * Aceita números positivos, negativos ou sem sinal explícito.
 * 
 * Parâmetros:
 *   - str: Ponteiro para uma string que será avaliada.
 * 
 * Return:
 *   - Retorna 1 (verdadeiro) se a string for um número inteiro válido.
 *   - Retorna 0 (falso) caso contrário.
 */

int verif_inteiro(const char *str) {
    // Verifica se a string está vazia.
    if (*str == '\0') return 0;

    // Ignora o sinal inicial '+' ou '-', se houver.
    if (*str == '-' || *str == '+') str++;

    // Itera por todos os caracteres da string.
    while (*str) {
        // Verifica se o caractere atual não é um dígito.
        if (!isdigit(*str)) return 0;
        str++;
    }

    // Se todos os caracteres forem dígitos, é um número inteiro válido.
    return 1;
}

int main(int argc, char **argv) {
    // Valida o número de argumentos
    if (argc != 4) {
        fprintf(stderr, "Erro: Numero de argumentos incorreto.\n");
        fprintf(stderr, "Usage: %s <diretorio_entrada> <integer> <flag>\n", argv[0]);
        return 1;
    }

    // Verifica se o segundo argumento é um número inteiro válido
    if (!verif_inteiro(argv[2])) {
        fprintf(stderr, "Erro: Segundo argumento (num_threads) deve ser um inteiro valido maior ou igual a 1.\n");
        return 1;
    }

    // Declaração de variáveis para medir tempo
    struct timespec start_time_total, end_time_total;
    struct timespec start_time_seq, end_time_seq;
    struct timespec start_time_par, end_time_par;

    // Marca o início do tempo total e sequencial
    clock_gettime(CLOCK_MONOTONIC, &start_time_total);
    clock_gettime(CLOCK_MONOTONIC, &start_time_seq);

// Diretório de entrada
    char *img_diretoria_path = argv[1];

    //Opção para deixar o programa mais apresentável.
    //printf("Diretório de Entrada (imagens originais): %s\n", img_diretoria_path);

	DIR *img_diretoria = opendir(img_diretoria_path);
	if (img_diretoria == NULL) {
        perror("Erro ao abrir o diretório de input. Por favor, verifique se esse diretório realmente existe.");
        return 1;
    }

// Obtém o número de threads e valida
    int n_threads = atoi(argv[2]);
    if (n_threads <= 0) {
        perror("Erro. Segundo argumento (num_threads) precisa ser um inteiro maior ou igual a 1.\n");
        return 1;
    }

// Obtém a flag de ordenação
    char *flag = argv[3];

// Declara variáveis para processar arquivos JPEG
    ImageInfo **img_arquivos = NULL;
	int img_contagem = 0;  // Contagem de ficheiros .jpeg encontrados

	struct dirent *entry;  // Ponteiro (diretório de entrada )

// Lê os arquivos do diretório
    while ((entry = readdir(img_diretoria)) != NULL) {
        // Verifica se o arquivo tem extensão .jpeg
        char *ext = strrchr(entry->d_name, '.');
        if (ext != NULL && strcmp(ext, ".jpeg") == 0) {

            // Constrói o caminho completo
            size_t full_path_size = strlen(img_diretoria_path) + strlen(entry->d_name) + 2;
            char *full_path = malloc(full_path_size);
            if (full_path == NULL) {
                perror("Erro de alocação de memória.");
            }
            snprintf(full_path, full_path_size, "%s/%s", img_diretoria_path, entry->d_name);

            // Obtém informações do arquivo
            struct stat file_info;
            if (stat(full_path, &file_info) == -1) {
                perror("Erro ao recuperar informações do arquivo.");
                closedir(img_diretoria);
                for (int i = 0; i < img_contagem; i++) {
                    free(img_arquivos[i]->nome);
                    free(img_arquivos[i]);
                }
                free(img_arquivos);
                return 1;
            }

// Realoca memória para armazenar mais um ponteiro para uma struct do tipo ImageInfo.
// Ajusta o tamanho do array de ponteiros img_arquivos para incluir o novo arquivo .jpeg identificado.
    img_arquivos = realloc(img_arquivos, (img_contagem + 1) * sizeof(ImageInfo *));
    if (img_arquivos == NULL) {
// Caso a realocação falhe, imprime uma mensagem de erro e libera recursos alocados até o momento.
        perror("Erro de alocação de memória.");
        closedir(img_diretoria);
        for (int i = 0; i < img_contagem; i++) {
            free(img_arquivos[i]->nome);
            free(img_arquivos[i]);
            }
        return 1; // Sai com erro
    }

// Aloca memória para armazenar uma nova struct ImageInfo, onde serão armazenados os dados do arquivo encontrado.
    img_arquivos[img_contagem] = malloc(sizeof(ImageInfo));
    if (img_arquivos[img_contagem] == NULL) {
// Caso a alocação falhe, imprime uma mensagem de erro e libera recursos alocados até o momento.
        perror("Erro de alocação de memória.");
        closedir(img_diretoria);
        for (int i = 0; i < img_contagem; i++) {
            free(img_arquivos[i]->nome);
            free(img_arquivos[i]);
        }
        free(img_arquivos); // Libera o array principal.
        return 1; // erro
    }

// Aloca memória para armazenar o nome do arquivo (string) encontrado e copia o nome do arquivo para a struct.
    img_arquivos[img_contagem]->nome = malloc(strlen(entry->d_name) + 1); // +1 para o terminador nulo '\0'.
    if (img_arquivos[img_contagem]->nome == NULL) {
// Caso a alocação da string falhe, imprime uma mensagem de erro e libera recursos já alocados.
    perror("Erro ao recuperar informações do arquivo.");
    closedir(img_diretoria);
    for (int i = 0; i < img_contagem; i++) {
        free(img_arquivos[i]->nome);
        free(img_arquivos[i]);
    }
    free(img_arquivos);
    return 1; // erro
} 

// Copia o nome do arquivo para o espaço de memória alocado na struct.
    strcpy(img_arquivos[img_contagem]->nome, entry->d_name);

    // Armazena o tamanho do arquivo no campo size da struct ImageInfo.
    img_arquivos[img_contagem]->size = file_info.st_size;

    // Incrementa o contador de arquivos .jpeg encontrados.
    img_contagem++;

// Libera a memória alocada para armazenar o caminho completo do arquivo após ser processado.
    free(full_path);
    }
    }

// Fecha o diretório de entrada após a leitura de todos os arquivos.
    closedir(img_diretoria);

// Exibe os arquivos .jpeg encontrados antes da ordenação.
// Opção de Para deixar o código mais apresentável, em contrapartida menos veloz.
    //printf("\nJPEG files found before sorting by %s:", flag);
    //for (int i = 0; i < img_contagem; i++) {
    //    printf("Nome: %s | Size: %ld bytes\n", img_arquivos[i]->nome, img_arquivos[i]->size);
    //}

// Verifica o valor da flag para o tipo de ordenação.
    if (strcmp(flag, "-name") == 0) {

        printf("\nComparação por nome.");

// Ordena os arquivos por nome.
        qsort(img_arquivos, img_contagem, sizeof(ImageInfo *), compara_nome);
    } else if (strcmp(flag, "-size") == 0) {

        printf("\nComparação por tamanho.");

// Ordena os arquivos por tamanho.
        qsort(img_arquivos, img_contagem, sizeof(ImageInfo *), compara_tamanho);
    } else {
// Se a flag for inválida, exibe uma mensagem de erro e encerra o programa.
        fprintf(stderr, "\nErro: flag invalida. Use -name ou -size.\n");
        return 1;
    }

// Exibe os arquivos .jpeg encontrados após a ordenação, no modo DEBUG.

    printf("\n Arquivos JPEG ordenados por %s: \n", flag);
    for (int i = 0; i < img_contagem; i++) {
        printf("nome: %s | Size: %ld bytes\n", img_arquivos[i]->nome, img_arquivos[i]->size);
    }


// Define o sufixo do diretório de saída, onde os arquivos processados serão armazenados.
    const char *suffix = "/old_photo_PAR_A/";

// Calcula o tamanho necessário para armazenar o caminho completo do diretório de saída.
    size_t diretorio_saida_size = strlen(img_diretoria_path) + strlen(suffix) + 1;

// Aloca memória para armazenar o caminho do diretório de saída.
    char *diretorio_saida = malloc(diretorio_saida_size);
    if (diretorio_saida == NULL) {
        perror("Erro de alocação de memória.");
        return 1;
    }

 // Concatena o caminho do diretório de entrada com o sufixo para formar o caminho completo do diretório de saída.
    strcpy(diretorio_saida, img_diretoria_path);
    strcat(diretorio_saida, suffix);

// Exibe o caminho resultante do diretório de saída, no modo DEBUG.

   // printf("Output directory: %s\n", diretorio_saida);


// Cria o diretório de saída.
    if (create_directory(diretorio_saida) == 0) {
        fprintf(stderr, "Impossível de criar %s diretorio\n", diretorio_saida);
        exit(-1);
    }

    gdImagePtr in_texture_img = read_png_file("./paper-texture.png");

// Declaração das threads e struct de dados associada a elas.
    pthread_t threads[n_threads];
    ThreadInfo thread_info[n_threads];

// Determina a divisão dos arquivos JPEG entre as threads.
    int files_per_thread = img_contagem / n_threads; // Número de arquivos por thread.
    int remainder = img_contagem % n_threads;        // Resto da divisão, para distribuir equitativamente.
    int start = 0;                                 // Índice inicial de processamento.

// Marca o tempo final para a execução sequencial e inicial para a execução paralela.
    clock_gettime(CLOCK_MONOTONIC, &end_time_seq);
    clock_gettime(CLOCK_MONOTONIC, &start_time_par);

 // Cria as threads para o processamento paralelo.
    for (int i = 0; i < n_threads; i++) {
        // 1º abordagem de divisão: distribui arquivos proporcionalmente entre as threads.
        int end = start + files_per_thread + (i < remainder ? 1 : 0);
        thread_info[i].start = start; // Índice inicial do intervalo de arquivos para a thread.
        thread_info[i].end = end;     // Índice final do intervalo de arquivos para a thread.

        // 2º abordagem de divisão: threads intercalam os arquivos.
        thread_info[i].img_contagem = img_contagem;
        thread_info[i].n_threads = n_threads; // Número total de threads.
        thread_info[i].thread_id = i;        // Identificador da thread.

        // Passa informações de diretórios, arquivos e textura para a thread.
        thread_info[i].img_arquivos = img_arquivos;
        thread_info[i].in_texture_img = in_texture_img;
        thread_info[i].diretorio_saida = diretorio_saida;
        thread_info[i].diretorio_entrada = img_diretoria_path;

        // Cria a thread para processar as imagens.
        pthread_create(&threads[i], NULL, process_imgs, &thread_info[i]);

        // Atualiza o índice inicial para a próxima thread.
        start = end;
    }

    // Variáveis para coletar os resultados das threads.
    void *thread_void_ret;
    ThreadResult thread_return[n_threads];

    // Aguarda a conclusão de todas as threads e coleta seus retornos.
    for (int i = 0; i < n_threads; i++) {
        pthread_join(threads[i], &thread_void_ret); // Espera a conclusão da thread.
        thread_return[i] = *(ThreadResult *)thread_void_ret; // Salva o retorno da thread em ThreadResult.
        free(thread_void_ret); // Libera a memória alocada para o retorno da thread.
    }

// Marca o tempo final da execução paralela e o tempo total.
    clock_gettime(CLOCK_MONOTONIC, &end_time_par);
    clock_gettime(CLOCK_MONOTONIC, &end_time_total);

// Calcula os tempos de execução paralela, sequencial e total.
    struct timespec par_time = diff_timespec(&end_time_par, &start_time_par);   // Tempo paralelo.
    struct timespec seq_time = diff_timespec(&end_time_seq, &start_time_seq);   // Tempo sequencial.
    struct timespec total_time = diff_timespec(&end_time_total, &start_time_total); // Tempo total.

// Criação do nome do arquivo para salvar os tempos de execução.
    char *time_file; // Ponteiro para armazenar o nome do arquivo.
    int time_file_size;

// Calcula o tamanho necessário para armazenar o nome do arquivo, incluindo o formato e os parâmetros.
    time_file_size = snprintf(NULL, 0, "timing_%d-%s.txt", n_threads, flag) + 1; 

// Aloca memória para o nome do arquivo.
    time_file = (char *)malloc(time_file_size * sizeof(char));
    if (time_file == NULL) { // Verifica se a alocação de memória foi bem-sucedida.
        fprintf(stderr, "Erro de alocação de memória.\n");
        return 1;
    }

// Cria o nome do arquivo com base no número de threads e na flag.
    snprintf(time_file, time_file_size, "timing_%d%s.txt", n_threads, flag);

// Calcula o tamanho necessário para o caminho completo do arquivo.
    size_t time_complete_path_size = diretorio_saida_size + time_file_size + 1;

// Aloca memória para o caminho completo do arquivo.
    char *time_complete_path = malloc(time_complete_path_size);
    if (time_complete_path == NULL) { // Verifica se a alocação foi bem-sucedida.
        perror("Erro de alocação de memória.");
        return 1;
    }

// Concatena o diretório de saída com o nome do arquivo.
    snprintf(time_complete_path, time_complete_path_size, "%s%s", diretorio_saida, time_file);

// Abre o arquivo para escrita.
    FILE *file = fopen(time_complete_path, "w");
    if (file == NULL) { // Verifica se o arquivo foi aberto corretamente.
        perror("Erro ao abrir arquivo.");
        return 1; 
    }


// Escreve os tempos de execução no arquivo.
// Tempo sequencial.
    fprintf(file, "\tseq \t %10jd.%09ld\n", seq_time.tv_sec, seq_time.tv_nsec);
// Tempo paralelo.
    fprintf(file, "\tpar \t %10jd.%09ld\n", par_time.tv_sec, par_time.tv_nsec);
// Tempo total.
    fprintf(file, "\ttotal \t %10jd.%09ld\n", total_time.tv_sec, total_time.tv_nsec);

// Escreve os tempos de cada thread no arquivo.
    for (int i = 0; i < n_threads; i++) {
                fprintf(file, "\tThread %d\t %10jd.%09ld\n", 
                thread_return[i].thread_id, // ID da thread.
                thread_return[i].thread_tempo.tv_sec, // Tempo em segundos.
                thread_return[i].thread_tempo.tv_nsec); // Tempo em nanossegundos.
    }

// Fecha o arquivo.
    fclose(file);

// Finaliza a execução do programa com sucesso.
    exit(0);
}