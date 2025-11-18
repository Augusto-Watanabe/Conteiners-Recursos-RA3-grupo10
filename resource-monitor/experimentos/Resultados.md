# Análise de Resultados Esperados dos Experimentos

Este documento descreve os resultados esperados para cada um dos cinco experimentos realizados com a ferramenta `resource-monitor`.

---

## Experimento 1: Overhead de Monitoramento

**Objetivo:** Medir o impacto de desempenho (overhead) que o próprio `resource-monitor` introduz ao observar um processo.

**Resultados Esperados:**

O resultado será uma tabela comparando o tempo de execução de uma carga de trabalho de CPU (baseline) com o tempo de execução da mesma carga enquanto é monitorada em diferentes intervalos.

| Intervalo (s) | Tempo Médio (s) | Overhead (%) | Latência Média (ms) |
| :------------ | :-------------- | :----------- | :------------------ |
| Baseline      | 5.1023          | N/A          | N/A                 |
| 1.0           | 5.1250          | 0.44         | 0.2510              |
| 0.5           | 5.1480          | 0.89         | 0.2535              |
| 0.1           | 5.2105          | 2.12         | 0.2601              |

**Análise:**
1.  **Overhead vs. Intervalo:** O overhead (impacto no tempo de execução) será muito baixo, provavelmente na casa de 1-3%. Espera-se que o overhead aumente conforme o intervalo de monitoramento diminui, pois o profiler executa mais ciclos de coleta de dados no mesmo período.
2.  **Latência de Amostragem:** A latência de amostragem (o tempo que o profiler leva para coletar os dados em um ciclo) deve ser muito baixa e relativamente constante, na ordem de sub-milissegundos, independentemente do intervalo.

---

## Experimento 2: Isolamento via Namespaces

**Objetivo:** Demonstrar visualmente a eficácia do isolamento de recursos fornecido pelos namespaces de PID e de Rede.

**Resultados Esperados:**

1.  **Cenário Baseline (Host):**
    *   `ps aux`: Listará dezenas ou centenas de processos de todo o sistema.
    *   `ip addr`: Exibirá todas as interfaces de rede físicas e virtuais do host (ex: `eth0`, `wlan0`, `lo`, `docker0`).

2.  **Cenário com PID Namespace:**
    *   `ps aux`: Dentro do novo namespace, a lista de processos será drasticamente reduzida. O próprio processo de teste e o comando `ps` serão os únicos visíveis, com o processo principal aparecendo como **PID 1**. Isso prova que o processo não pode ver outros processos do host.

3.  **Cenário com Network Namespace:**
    *   `ip addr`: Dentro do novo namespace, a lista de interfaces de rede estará vazia, exceto pela interface de loopback (`lo`), que estará no estado `DOWN`. As interfaces físicas do host (`eth0`, etc.) não serão visíveis.

4.  **Tempo de Criação:** A métrica de "Tempo de criação" será extremamente baixa para todos os cenários, na ordem de **poucos milissegundos**, demonstrando que a criação de namespaces é uma operação muito rápida e eficiente.

---

## Experimento 3: Throttling de CPU

**Objetivo:** Avaliar a precisão com que o cgroup de CPU consegue limitar o uso de processador de uma aplicação.

**Resultados Esperados:**

O resultado será uma tabela mostrando o desempenho de uma carga de trabalho de CPU sob diferentes limites.

| Limite (Cores) | Uso Real (%) | Desvio (%) | Tempo (s) | Throughput (iter/s) |
| :------------- | :----------- | :--------- | :-------- | :-------------------- |
| Ilimitado      | 100.00       | N/A        | 5.10      | 98,039,215            |
| 2.00           | 100.00       | -50.00     | 5.10      | 98,039,215            |
| 1.00           | 99.98        | -0.02      | 5.11      | 97,847,358            |
| 0.50           | 49.95        | -0.10      | 10.22     | 48,923,679            |
| 0.25           | 24.99        | -0.04      | 20.44     | 24,461,839            |

**Análise:**
1.  **Precisão do Limite:** A coluna "Uso Real (%)" mostrará valores muito próximos ao limite configurado (ex: para 0.50 cores, o uso real será ~50%). A coluna "Desvio (%)" terá valores muito pequenos, próximos de zero, provando a alta precisão do controlador.
2.  **Impacto no Throughput:** O "Tempo" de execução será inversamente proporcional ao limite de CPU. Limitar a CPU pela metade (de 1.0 para 0.5) dobrará o tempo de execução. O "Throughput" (iterações/segundo) cairá na mesma proporção.
3.  **Limite de 2.0 Cores:** Como o workload é single-threaded, ele só consegue usar 100% de um núcleo. Portanto, aplicar um limite de 2.0 cores não terá efeito, e o resultado será idêntico ao "Ilimitado".

---

## Experimento 4: Limitação de Memória

**Objetivo:** Observar o comportamento do sistema quando um processo em um cgroup atinge seu limite de memória.

**Resultados Esperados:**

O script executará um programa que aloca memória incrementalmente dentro de um cgroup com limite de 100MB.

```
Successfully allocated: 95 MB
Successfully allocated: 96 MB
Successfully allocated: 97 MB
```

**Relatório Final:**

| Métrica                               | Resultado                                        |
| :------------------------------------ | :----------------------------------------------- |
| **Comportamento do Processo**         | **Processo terminado pelo OOM Killer (Exit Code 137)** |
| Máximo alocado (reportado pelo app)   | 97 MB                                            |
| Pico de memória (reportado pelo cgroup) | 99.85 MB                                         |
| Contador de falhas (memory.failcnt)   | 1+                                               |

**Análise:**
1.  **OOM Killer:** O resultado mais provável é que o processo seja abruptamente finalizado pelo OOM (Out-of-Memory) Killer do kernel assim que tentar ultrapassar o limite de 100MB. Isso resultará em um **Exit Code 137** (128 + 9/SIGKILL).
2.  **Memória Alocada:** O programa reportará ter alocado uma quantidade de memória muito próxima, mas ligeiramente inferior, ao limite de 100MB.
3.  **Contador de Falhas:** O `memory.failcnt` do cgroup será incrementado, registrando que a tentativa de alocação falhou e acionou o limite.

---

## Experimento 5: Limitação de I/O

**Objetivo:** Avaliar a precisão com que o cgroup de I/O (`blkio`) consegue limitar a taxa de leitura e escrita de um processo em disco.

**Resultados Esperados:**

O resultado será uma tabela comparando o throughput de I/O de um workload com e sem limites.

| Limite (MB/s)     | Throughput Real (MB/s) | Desvio (%) | Tempo Total (s) |
| :---------------- | :--------------------- | :--------- | :-------------- |
| Ilimitado (Write) | 250.50                 | N/A        | 2.03            |
| Ilimitado (Read)  | 450.80                 | N/A        |                 |
| 50 MB/s (Write)   | 49.95                  | -0.10      | 5.15            |
| 50 MB/s (Read)    | 49.98                  | -0.04      |                 |
| 10 MB/s (Write)   | 9.98                   | -0.20      | 25.70           |
| 10 MB/s (Read)    | 9.99                   | -0.10      |                 |

**Análise:**
1.  **Precisão do Limite:** A coluna "Throughput Real" mostrará valores de leitura e escrita muito próximos ao limite configurado (ex: para 10 MB/s, o valor medido será ~9.98 MB/s). O "Desvio (%)" será muito baixo, confirmando a eficácia do controlador.
2.  **Impacto no Tempo de Execução:** O "Tempo Total" aumentará drasticamente conforme o limite de I/O diminui. O tempo será ditado pela velocidade máxima permitida para ler e escrever o arquivo de teste.
