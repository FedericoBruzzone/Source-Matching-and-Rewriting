#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NULL ((void*)0)
typedef unsigned long size_t;  
typedef long intptr_t; 
typedef unsigned long uintptr_t;
typedef long scalar_t__;  
typedef int bool;
#define false 0
#define true 1

// --- Implementação das funções auxiliares de memória ---

double **mm_init(int n) {
    double **matrix = (double **)malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++) {
        matrix[i] = (double *)calloc(n, sizeof(double)); // Inicializa com zeros
    }
    return matrix;
}

void mm_destroy(int n, double **matrix) {
    for (int i = 0; i < n; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

// --- Sua função original ---

double **mm_mul(int size, double *const *a, double *const *b)
{
    int i, j, k, n = 1000;
    double **m, **c;
    m = mm_init(n); c = mm_init(n);
    for (i = 0; i < n; ++i) // transpose
        for (j = 0; j < n; ++j)
            c[i][j] = b[j][i];
    for (i = 0; i < n; ++i) {
        double *p = a[i], *q = m[i];
        for (j = 0; j < n; ++j) {
            double t = 0.0, *r = c[j];
            for (k = 0; k < n; ++k)
                t += p[k] * r[k];
            q[j] = t;
        }
    }
    mm_destroy(n, c);
    return m;
}

// --- Função Main ---

int main() {
    // Definindo o tamanho da matriz (Ex: 3x3)
    // Dica: Para ver tempos maiores de execução, aumente este número (ex: 500 ou 1000)
    int n = 1000; 

    // Alocando e inicializando as matrizes A e B
    double **matrix_A = mm_init(n);
    double **matrix_B = mm_init(n);

    // Preenchendo as matrizes com valores de teste
    // Matriz A receberá valores sequenciais, Matriz B será uma Matriz Identidade
    // Multiplicar qualquer matriz pela identidade deve resultar nela mesma.
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            matrix_A[i][j] = i * n + j + 1;       // Ex: 1, 2, 3...
            matrix_B[i][j] = (i == j) ? 1.0 : 0.0; // Matriz Identidade
        }
    }

    printf("--- Matriz A ---\n");
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) printf("%6.1f ", matrix_A[i][j]);
        printf("\n");
    }

    printf("\n--- Matriz B (Identidade) ---\n");
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) printf("%6.1f ", matrix_B[i][j]);
        printf("\n");
    }

    printf("\nExecutando mm_mul...\n");

    // --- MEDIÇÃO DE TEMPO INÍCIO ---
    clock_t start_time = clock();

    double **matrix_R = mm_mul(n, matrix_A, matrix_B);

    clock_t end_time = clock();
    // --- MEDIÇÃO DE TEMPO FIM ---

    // Cálculo do tempo total em segundos
    double execution_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    printf("\n--- Matriz Resultado ---\n");
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            printf("%6.1f ", matrix_R[i][j]);
        }
        printf("\n");
    }

    printf("\n=========================================\n");
    printf("Tempo de Execução: %f segundos\n", execution_time);
    printf("=========================================\n");

    // Liberando a memória alocada
    mm_destroy(n, matrix_A);
    mm_destroy(n, matrix_B);
    mm_destroy(n, matrix_R);

    return 0;
}