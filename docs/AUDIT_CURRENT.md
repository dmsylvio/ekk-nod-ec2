# Revisão do código após os primeiros lotes de correções

Base analisada: `c9c42e23323d7f1c5bd29064553e42af061c4098`.
Data: 2026-11-10. Método: revisão estática dirigida e testes locais com memória simulada.

Esta revisão não afirma encontrar todos os erros e não confirma os 88 itens da
lista original em bloco. Bibliotecas de terceiros não receberam revisão completa.
Não houve execução contra CS2, kernel, EFI, DMA ou servidores remotos. Não foram
atualizadas assinaturas, índices ou offsets. As referências são relativas à base
acima. Falhas de inicialização não provam que o processo está ausente.

## Evidências reproduzidas

1. `vmwin::dump_module`: com SizeOfImage=1 e SizeOfHeaders=32 em memória simulada,
   AddressSanitizer detecta escrita além do heap em memcpy. O buffer tem 17 bytes
   incluindo os 16 bytes de metadados. Uma leitura remota bem-sucedida consegue
   exceder a área destinada à imagem. Nenhum processo real foi acessado.
2. `vmwin::scan_pattern`: dump sintético com um byte de imagem causa leitura além
   do heap ao acessar e_lfanew, antes de validar os cabeçalhos. ASan aponta a linha
   283 de `library/vm/shared/vm_windows.h`.
3. `vmlinux::get_module_export`: com todas as leituras simuladas retornando zero,
   não termina em dois segundos; o teste foi encerrado por timeout. O laço compara
   um BOOL com -1 e continua após falhas.
4. `math::qfloor(-2)` retorna -3; `qatan2(0,0)` retorna NaN;
   `qpow(2,-1)` retorna 1 e `qpow(4,0.5)` retorna 4.
5. Os testes existentes de memória, CS2 e enumeração UM passaram. Eles exercitam
   casos específicos e não cobrem a inicialização completa ou dados PE/ELF hostis.

## Problemas confirmados no código atual

“Confirmado” nesta tabela significa que o caminho defeituoso é verificável no
código; somente os itens da seção anterior tiveram reprodução dinâmica nesta
revisão. Gravidade depende de onde os dados são obtidos e do backend escolhido.

| ID | Prioridade | Local | Defeito e consequência |
|---|---|---|---|
| R01 | Alta | `cs2/shared/cs2.cpp:299–430,624–627` | Regressão do lote anterior: descoberta Linux não atribui m_bIsDefusing, m_bPawnHasDefuser, m_hActiveWeapon e m_pWeaponServices, mas a validação agora exige todos. Com reset inicial em zero, a inicialização Linux sempre falha. |
| R02 | Crítica | `library/vm/shared/vm_windows.h:154–173` | SizeOfHeaders não é limitado por SizeOfImage antes da cópia. Escrita fora do heap reproduzida. |
| R03 | Crítica | `library/vm/shared/vm_windows.h:211–214` | RVA/offset e VirtualSize controlam destino e tamanho de cópia sem validar a região alocada. |
| R04 | Alta | `library/vm/shared/vm_windows.h:172–216` | Falhas nas leituras de cabeçalhos/seções são ignoradas; o dump pode ser retornado como sucesso com conteúdo não inicializado ou incompleto. |
| R05 | Alta | `library/vm/shared/vm_windows.h:160–161` | Em kernel, image_size+16 é calculado como DWORD antes do cast; pode dar wraparound na alocação. |
| R06 | Crítica | `library/vm/shared/vm_windows.h:233–269` | get_dump_export desreferencia cabeçalhos, tabelas, ordinais e strings sem conferir limites do dump; entrada nula também não é rejeitada. |
| R07 | Alta | `library/vm/shared/vm_windows.h:100,242` | e_lfanew é lido como WORD em duas rotinas, truncando um campo de 32 bits. |
| R08 | Crítica | `library/vm/shared/vm_windows.h:282–299` | Busca lê cabeçalhos e confia nos limites das seções antes de validar o tamanho do dump. O conserto do comparador não resolve limites falsos fornecidos pelo parser. |
| R09 | Alta | `library/vm/shared/vm_linux.h:100–113` | Comparação de retorno BOOL com -1 impede a parada por erro; ausência de limite de símbolos permite execução indefinida. |
| R10 | Alta | `library/vm/shared/vm_linux.h:103–107` | Uma leitura que preencha 120 bytes não garante terminador da string antes da comparação. |
| R11 | Alta | `library/vm/shared/vm_linux.h:109` | Leitura ELF32 escreve quatro bytes sobre sym_tab de oito bytes sem limpar a metade superior. |
| R12 | Alta | `library/vm/shared/vm_linux.h:120–129` | Cabeçalho ELF não é inicializado, sucesso da leitura não é verificado e somas de tamanho não são validadas antes da alocação. |
| R13 | Alta | `cs2/shared/cs2.cpp:299–606` | Descoberta de netvars lê valores por casts desalinhados, permite ponteiro igual ao fim do dump e compara strings sem limite. Há leituras de campos posteriores sem reservar espaço para eles. |
| R14 | Média | `cs2/shared/cs2.cpp:286–296` | Log “game is running” ocorre antes da descoberta/validação final. Pode anunciar sucesso e logo depois retornar falha. |
| R15 | Média | `cs2/shared/cs2.cpp:engine::get_convar` | Copia somente o comprimento procurado e acrescenta zero. Um nome remoto com o mesmo prefixo e sufixo adicional pode ser aceito como igual. O overflow foi corrigido, a igualdade completa não. |
| R16 | Média | `cs2/shared/cs2.cpp:entity::is_player` | Retorna verdadeiro para qualquer controller não nulo; não valida o tipo. |
| R17 | Alta | `cs2/shared/cs2.cpp:920–939` | Contagem externa de aim-punch não tem limite superior; a multiplicação por 12 pode dar wraparound antes da soma ao endereço. |
| R18 | Média | `cs2/shared/cs2.cpp:1067–1096` | Um node nulo ou leitura de dormancy que falha produz zero e pode ser aceito como entidade válida se os demais campos passarem. |
| R19 | Média | `cs2/shared/cs2.cpp:entity::get_client_entity,node::get_bone_position` | Índices negativos e produto index*32 não são validados; há deslocamentos inválidos e risco de overflow de inteiro com sinal. |
| R20 | Alta | `cs2/shared/cs2features.cpp:357,520–521,693–706` | Sensibilidade zero por falha de leitura chega à divisão e conversão para int, sem teste de valor finito/faixa. |
| R21 | Média | `cs2/shared/cs2features.cpp:45–54` | reset zera temporizadores sem liberar explicitamente um botão sintético pressionado; event_state também não é reinicializado. |
| R22 | Média | `library/math.h:152–157` | qfloor erra inteiros negativos e converte float para int sem validar domínio/faixa. |
| R23 | Média | `library/math.h:59–82` | qatan2(0,0) divide por zero e produz NaN. |
| R24 | Média | `library/math.h:90–104` | qacos não limita entradas ao domínio [-1,1]. |
| R25 | Baixa | `library/math.h:109–149` | qpow não implementa expoentes negativos/fracionários; aproximações de seno/cosseno não reduzem o argumento. |
| R26 | Alta | `library/vm/km/vm.cpp:118–208` | Leituras/escritas pulam páginas não traduzidas ou interrompem por erro, mas retornam sucesso incondicional. |
| R27 | Alta | `library/vm/ksj/src/vm.cpp:126–164` | open_process pode retornar &system_proc, objeto estático; close chama free sem distinguir esse caso. |
| R28 | Alta | `library/vm/ksj/src/vm.cpp:283–298,374–460` | Transferências de socket não repetem envio/recepção parcial; erros retornados como QWORD perdem o contrato de valor negativo. |
| R29 | Alta | `library/vm/ksj/src/vm.cpp:376–388,442–455` | Soma length+header sem verificar overflow, malloc sem verificar resultado e tamanho de protocolo truncado para DWORD. |
| R30 | Média | `library/vm/linux/vm.cpp:30–47`, `library/vm/proton/vm.cpp` | readlink não fornece terminador; o código usa buffer como string sem conferir tamanho retornado. Caminho do link e destino compartilham buffer. |
| R31 | Média | `library/vm/linux/vm.cpp:57–70`, `library/vm/proton/vm.cpp` | read_line interpreta read=-1 como condição verdadeira e não retorna corretamente uma última linha sem newline. |
| R32 | Média | `library/vm/linux/vm.cpp:182–184`, `library/vm/proton/vm.cpp:running` | Descritor aberto não prova que o processo original continua vivo. |
| R33 | Alta | `library/vm/sgv/vm.cpp:164–312` | Enumeração ainda começa por Process32Next e não trata snapshot inválido. A correção publicada atingiu somente UM. |
| R34 | Crítica | `projects/DMA/main.cpp:277,329` | recvfrom autoriza 1024 bytes para objeto rx menor; falta limitar pelo sizeof do destino. |
| R35 | Crítica | `projects/KM/main.cpp:305–312`, `projects/EFI/bootx64/kernel.cpp` | Tamanho da extensão é dividido por 4 mas indexado como ponteiros; leitura i+1 não reserva o último elemento. |
| R36 | Alta | `projects/KM/main.cpp:227–239` | Retornos de criação/referência de thread são ignorados antes de anunciar sucesso. |
| R37 | Crítica | `projects/EFI/bootx64/main.cpp:332–334` | Exportação não validada vira destino de escrita. |
| R38 | Alta | `projects/EFI/bootx64/main.cpp:219–224` | Falha de inicialização retorna zero (sucesso EFI) sem chamar ExitBootServices original. |
| R39 | Alta | `projects/EFI/bootx64/main.cpp:434–442` | Falha persistente de AllocatePages não tem limite de tentativas ou saída. |
| R40 | Média | `projects/KM/km.vcxproj`, `projects/EFI/bootx64/*.vcxproj` | /GS e CFG estão explicitamente desativados. A compatibilidade de cada mitigação com cada alvo precisa ser avaliada antes de habilitar; SafeSEH não é mitigação aplicável a x64. |
| R41 | Média | `csgo/shared/csgofeatures.cpp:1036–1039` | Fallback da resolução escreve x duas vezes, deixando y sem correção. |
| R42 | Média | `apex/shared/apexfeatures.cpp:624–625` | max_health é divisor sem teste contra zero/falha de leitura. |

## Riscos dependentes do ambiente

- Assinaturas, índices virtuais, lifeState=256 e offsets fixos podem estar
  incompatíveis com uma versão do binário. A revisão não demonstra quais valores
  atuais estão incorretos. Não se deve substituí-los por palpites.
- PROCESS_ALL_ACCESS pode falhar por permissões; isso é hipótese de diagnóstico,
  não evidência de que esse seja o motivo observado pelo usuário.
- Compartilhamento de buffers, handles e sockets sem sincronização requer revisão
  de concorrência e do modelo de chamadas antes de afirmar uma corrida observada.
- O protocolo UDP não valida adequadamente o remetente em send; autenticidade,
  timeout e comportamento diante de pacotes inesperados precisam de testes próprios.
- ABI, representação de tipos, libs incluídas, SDL, builds Windows e comportamento
  após atualizações de drivers/firmware não foram validados neste ambiente Linux.

## Correções já presentes

- Fallback Windows da matriz não é mais abortado pelo primeiro JZ.
- FindPatternEx recebe a região completa, considera o comprimento da máscara e
  inclui a última posição válida. A validação dos limites PE continua pendente.
- Terminador de nome de exportação não escreve mais no índice 120.
- free_module(nullptr) é seguro nos helpers Windows e Linux.
- Reset limpa os campos de sessão do cs2.cpp e fecha o handle.
- get_current_ms não mantém mais o deslocamento em static local.
- Nomes, contagens de convars/fontes de mouse e tabela de função têm verificações
  adicionais. Isso não torna todas as cadeias de endereço do módulo seguras.
- Enumeração UM usa Process32FirstA/Process32NextA e verifica snapshot.
- Diagnósticos JZ mostram expressão e linha; falhas internas do parser ainda não
  têm códigos de erro distintos.

## Imprecisões da auditoria original

- O item 33 ocorre no buffer do nome da exportação em get_module_export;
  classificá-lo como buffer do nome do módulo era impreciso.
- KSJ aloca com tamanho QWORD, não simplesmente com comprimento truncado para
  DWORD. O truncamento está no campo de protocolo; somas e transmissões continuam
  exigindo validação, mas são mecanismos diferentes.
- Não reproduzi NaN em vec_delta com pontos coincidentes: o teste atual retornou
  zero devido à aproximação de qsqrt. O domínio degenerado precisa de contrato
  explícito, mas o resultado exato alegado anteriormente não deve ser presumido.
- O readlink não terminado só vira acesso além do buffer em determinadas entradas;
  ausência de terminador por si só não demonstra que toda execução ultrapassa a área.
- O número de artefatos/tamanho da auditoria original não foi recontado. Não usar
  59 arquivos/128 MB como inventário atual verificado.

## Ordem de investigação e correção

1. Tratar R01 sem permitir que campos ausentes sejam usados como offsets zero:
   definir campos obrigatórios por plataforma e desabilitar getters/recursos opcionais.
2. Validar PE/ELF antes de qualquer dereferência/cópia; parar por leitura parcial;
   adicionar testes dos três casos reproduzidos e fuzzing com limites de recursos.
3. Remover descoberta de netvars por leituras sem limites; distinguir ausência,
   leitura incompleta, versão incompatível e valor válido zero.
4. Validar resultados numéricos e estado de input/reset antes de testes funcionais.
5. Corrigir limites de rede e contratos de backends; manter kernel/EFI fora dos
   testes de execução até revisão específica.
6. Obter do usuário backend, mensagem completa e versão do executável recompilado.
   Sem isso, a causa do problema no computador dele continua não confirmada.

Não houve correção de código nesta revisão. O relatório registra 42 grupos de
problemas, alguns sobrepostos aos 88 itens originais; não são 42 defeitos novos nem
uma contagem completa do projeto. Os demais itens originais permanecem pendentes
de validação individual, não implicitamente resolvidos.
