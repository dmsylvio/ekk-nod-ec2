# Estado das correções da auditoria

Esta rodada continua a revisão registrada em AUDIT_CURRENT.md. O relatório anterior
é evidência histórica da base c9c42e2, não o estado posterior a este lote.
Não há alegação de que todos os erros do projeto foram eliminados.

## Alterações nesta rodada

- PE: validação de MZ/PE, campos de tamanho, cabeçalhos opcionais e tabela de
  seções; aritmética antes da alocação em QWORD; cópias limitadas à imagem;
  interrupção por erro de leitura; buffer zerado; exports e buscas locais com
  leituras limitadas, sem casts desalinhados; e_lfanew de 32 bits; rejeição de
  exports encaminhados, que exigem resolvedor próprio.
- ELF: leitura verificada de cabeçalhos e segmentos ELF32/ELF64; término e
  orçamento de trabalho nas tabelas dinâmicas/símbolos; strings terminadas;
  valores de símbolos ELF32 inicializados; dump baseado em PT_LOAD com limites
  de tamanho e releitura validada. Raw ELF retorna falha explícita: reconstrução
  de arquivo não é equivalente a copiar uma imagem carregada.
- CS2: leituras locais de netvars via cópias limitadas, comparação de strings
  dentro do dump, limites dos loops de seção; validação obrigatória por plataforma;
  getters dos quatro campos ausentes no Linux desabilitados; mensagem de sucesso
  após validação; igualdade completa de convars; índices e contagens limitados;
  dormancy falha como inválido; descarte do cache de bone ao trocar alvo;
  liberação de botão no reset e limites antes de converter movimento para int.
- Matemática: sqrt sem type-punning, tratamento de zero e subnormais, floor de
  negativos, atan2 degenerado, clamp de acos, proteção de vetor nulo e projeção
  não finita, norma correta no cálculo angular. Builds comuns usam pow/sin/cos
  da biblioteca padrão. Kernel mantém implementação sem CRT e qpow aceita apenas
  expoentes inteiros entre -1024 e 1024; fora disso retorna NaN. Seno/cosseno kernel reduzem o argumento e rejeitam
  magnitude superior a 1e12; fatorial aceita inteiros de 0 a 170.
- Linux/Proton: readlink com buffer separado e terminador, DT_UNKNOWN aceito,
  leitor de linhas que trata erro/EOF/truncamento; handles tipados com PID,
  descritor e instante de início para detectar término e reutilização de PID.
  Linux define as operações PE não aplicáveis como retorno zero explícito.
- KSJ: proteção do objeto estático contra free, limites e verificações de malloc,
  transferência completa com validação do retorno, serialização de transações e
  bloqueio de novas operações após transporte interrompido. Uma sessão de socket
  comprometida requer reinício; não há reconexão transparente implementada.
- KM: loops de cópia não declaram sucesso para páginas faltantes ou falhas
  parciais; buffer físico global removido; aritmética de limites sem soma que
  possa dar wraparound.
- Ajustes pontuais ainda sem build nativo: enumeração SGV; byte count em PCILeech;
  sizeof(rx) no UDP DMA; contagem/último índice da extensão em KM/EFI; exportação,
  alocação de tabela e tentativas EFI verificadas; retorno original de
  ExitBootServices preservado; fallback de resolução CSGO e divisor Apex.

Limites (512 MiB/imagem, 96 seções PE, 1024 segmentos ELF, 1 Mi símbolos,
256 tentativas EFI, 4096 entradas de aim-punch/bones) são orçamentos defensivos,
não propriedades verificadas de uma versão do jogo ou firmware. Podem causar
rejeição de entradas legítimas maiores e exigem revisão de compatibilidade.
A API continua exigindo que dumps sejam alocações válidas produzidas pela
biblioteca, com os 16 bytes de metadados; não aceita ponteiros arbitrários.

## Validação executada

`bash tests/run_safety.sh`: sete executáveis de teste com ASan/UBSan, um teste da
matemática com _KERNEL_MODE, enumeração UM simulada, contrato KM com tradução de
páginas simulada e checagem de sintaxe dos módulos listados no script.

Casos incluem imagens válidas, dumps truncados, cabeçalhos/seções fora da imagem,
falhas de leitura, strings nos limites, valores matemáticos extremos, reset de
input, handles do próprio processo de teste, EOF/erro/linhas longas e socketpair
local com envio dividido/EOF parcial. Nenhum jogo, driver, firmware ou dispositivo
DMA foi executado. O teste com _KERNEL_MODE não é compilação WDK.

## Relação com os 42 grupos

| IDs | Estado nesta rodada |
|---|---|
| R01–R12 | Correções implementadas nos caminhos de inicialização e parsers; testes sintéticos passaram. Compatibilidade nativa e fuzzing extensivo pendentes. |
| R13 | Leituras e strings locais limitadas; esquema versionado e descoberta tipada continuam pendentes. |
| R14–R15 | Corrigidos no código; igualdade completa de convars coberta por teste. |
| R16 | Pendente: não há esquema verificado para identificar tipo de entidade. Não foi inventado um índice/offset. |
| R17–R24 | Proteções implementadas e testes de limites/reset/matemática; integração ao jogo pendente. |
| R25 | Corrigido em builds comuns; contrato kernel restrito e explícito, sem matemática fracionária geral. |
| R26–R29 | Contratos e transporte corrigidos nos casos testados; KM requer build WDK, KSJ requer testes com servidor real e lifecycle completo. |
| R30–R32 | I/O/handles corrigidos; testes locais passaram. Proton continua com descoberta de processo/módulo específica e ainda não validada. |
| R33–R35 | Correções pontuais implementadas; compilação/testes Windows, hardware e layouts internos pendentes. |
| R36 | Pendente: recuperação segura após falha de criação/referência de thread exige revisar o ciclo de descarregamento do driver. |
| R37–R39 | Guardas e controle de fluxo corrigidos; bootstrap/mapeamento e execução EFI não validados. |
| R40 | Pendente: política de mitigação por alvo e toolchain. Não foram ativadas opções incompatíveis às cegas. |
| R41–R42 | Correções pontuais implementadas; builds nativos e integração pendentes. |

## Pendências que impedem declarar “todos corrigidos”

- Build Windows/WDK/EFI e teste em VM descartável, inclusive descarregamento,
  erro de alocação, boot e falhas intermediárias. Este ambiente tem GCC/Linux,
  não o toolchain WDK/firmware e o hardware dos projetos.
- Identificação de entidade e compatibilidade de layouts, lifeState, offsets e
  índices por versão; backend Proton ainda contém escolhas específicas de outro
  executável/módulo. Não há dados verificados para substituir esses valores.
- Contrato público estruturado para diferenciar zero válido, erro e cópia parcial;
  revisão integral das demais cadeias de ponteiro e recursos opcionais.
- Autenticidade, timeout e validação semântica do UDP; gerenciamento completo de
  Winsock e demais recursos; referências de objetos SGV; lifecycle dos outros jogos;
  SDL; builds reproduzíveis; inventário/remoção de artefatos; matriz de plataformas.
- Os demais itens dos 88 achados originais não foram todos revalidados e corrigidos.
  A lista de 42 grupos não substitui aquele inventário nem constitui prova de
  ausência de outros defeitos.
