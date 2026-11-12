# EKK NOD EC2

Projeto em C++ para estudar ferramentas externas de Counter-Strike 2 em partidas locais com bots. O executável **UM** roda em modo usuário no Windows e reúne uma sobreposição SDL, mira automática e trigger configuráveis.

O projeto também serve como base para estudar detecção de trapaças. Os controles documentados aqui são recursos do cliente de teste. O bloqueio de dados por distância no servidor continua sendo uma ideia de pesquisa.

## Requisitos

- Windows x64.
- Visual Studio 2022 com **Desktop development with C++**, toolset **MSVC v143** e Windows SDK.
- Counter-Strike 2 com a build interna **14196** (Steam build **26445667**). O código verifica a build antes de usar a matriz de projeção. Atualizações do jogo podem exigir ajustes.
- A `SDL3.dll` incluída em `projects/UM`, ao lado do executável durante a execução.

## Windows x64

Baixe **ekk-windows-x64** em **Artifacts** de uma execução bem-sucedida do [workflow Windows x64](https://github.com/dmsylvio/ekk-nod-ec2/actions/workflows/windows-x64.yml). Extraia o ZIP e execute `um.exe` mantendo `SDL3.dll` na mesma pasta.

### Compilar localmente

1. Abra [`projects/UM/um.sln`](projects/UM/um.sln) no Visual Studio.
2. Selecione **Release** e **x64** e compile a solução.
3. Copie `projects/UM/x64/Release/um.exe` para `projects/UM`, onde está a `SDL3.dll`.
4. Abra uma partida local com bots no CS2, de preferência em **janela sem bordas**, e execute `um.exe`.

Também é possível compilar a partir da raiz do repositório no **Developer PowerShell for VS 2022**:

```powershell
msbuild .\projects\UM\um.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64
Copy-Item .\projects\UM\x64\Release\um.exe .\projects\UM\um.exe
```

## Painel e controles

O painel **CS2 Controls** abre junto com o executável. Use **F8** para mostrar ou esconder o painel. Em **Key Bindings**, escolha as teclas desejadas. As alterações são salvas em `um-settings.ini`, na pasta do executável.

| Controle | Padrão | Comportamento |
| --- | --- | --- |
| **Aim (Hold)** | Mouse 5 | Ajusta a mira enquanto a tecla estiver pressionada. Mouse 1 também pode ser selecionado. |
| **Trigger (Hold)** | Mouse 4 | Dispara enquanto a tecla estiver pressionada e houver um alvo válido sob a mira. Espera de 65 a 110 ms ao adquirir o alvo e mantém pelo menos 140 ms entre tiros. |
| **Wall (Toggle)** | F7 | Liga ou desliga os retângulos da sobreposição. |

Em **Aim Regions**, configure as chances de seleção de **Head**, **Body** e **Arms / Legs**. Os valores iniciais são **40%**, **50%** e **10%**. A soma deve ser 100%. Clique em **Apply regions** para salvar.

Use **Aim strength** para ajustar a força da mira de 0 a 100%. O valor é salvo em `um-settings.ini`. Com 0%, o Aim fica desligado.

Cada escolha aponta para um *bone* (um ponto do esqueleto do modelo). A *hitbox* é o volume usado pelo jogo para registrar um acerto. A distribuição real dos acertos também depende da posição e do movimento dos alvos.

## Compatibilidade com builds do CS2

Em **Game Compatibility**, o modo **Automatic** usa a única instância do CS2 ou um caminho preferido que corresponda a uma única instância aberta. Se houver mais de uma opção, escolha pelo PID e caminho. O caminho escolhido fica salvo em `um-settings.ini`.

Na primeira execução, o programa cria `um-profiles.ini` ao lado de `um.exe`. O painel mostra o PID, a build interna e se o perfil foi carregado. O botão **Reload** atualiza a lista e relê o arquivo. Um perfil ausente ou inválido impede a inicialização.

Para uma build nova, adicione uma seção `[Build.<número interno>]` com os campos do exemplo gerado. Use somente valores verificados para essa build. Se o local do número da build no módulo do jogo mudar, atualize também `EngineBuildRva` em `[Detection]`. Mudanças na estrutura das convars, das entidades ou dos campos do jogo podem exigir ajustes no código.

## Verificação e diagnóstico

A compilação **Release x64** e os testes com memória simulada foram executados no Windows. A aparência do painel e o comportamento de mira e trigger devem ser conferidos na partida local.

- Se o executável falhar ao abrir, confira se a `SDL3.dll` está ao lado dele.
- Se a inicialização falhar, consulte as mensagens `[EC]` no console. `CS2 initialization complete` indica que o módulo foi inicializado.
- Se a sobreposição não aparecer, confira o estado de **Wall (Toggle)** e o modo de exibição do jogo.

## Linux x64

O [workflow Linux x64](https://github.com/dmsylvio/ekk-nod-ec2/actions/workflows/linux-x64.yml) compila e empacota o executável no GitHub Actions. Abra a execução bem-sucedida mais recente e baixe **ekk-linux-x64** em **Artifacts**. O pacote inclui o executável, a biblioteca SDL3 e o painel de configuração Linux.

Depois de extrair o ZIP baixado do GitHub, execute:

```sh
tar -xzf ekk-linux-x64.tar.gz
cd ekk-linux-x64
./ekk-linux
```

A sobreposição usa X11 ou XWayland. Para abrir o painel, execute `./ekk-linux --settings`. O painel usa Python 3, PyGObject e GTK 4. No Ubuntu, instale essas dependências com `sudo apt install python3-gi gir1.2-gtk-4.0`.

### Permissão persistente do mouse

Aim e Trigger precisam de acesso de leitura e escrita ao dispositivo de eventos do mouse. Para um **SteelSeries Rival 3 Wireless**, crie uma regra `udev` que concede acesso à sessão local ativa:

```sh
sudo tee /etc/udev/rules.d/71-ekk-mouse.rules >/dev/null <<'EOF'
SUBSYSTEM=="input", KERNEL=="event*", ENV{ID_INPUT_MOUSE}=="1", ENV{ID_SERIAL}=="SteelSeries_SteelSeries_Rival_3_Wireless", TAG+="uaccess"
EOF
sudo udevadm control --reload-rules
```

Reconecte o receptor USB e execute o EKK como seu usuário normal. Para outro modelo, consulte `udevadm info --query=property --name=/dev/input/by-id/<dispositivo>-event-mouse` e substitua o valor de `ID_SERIAL` na regra.

## Estrutura do repositório

| Pasta | Conteúdo |
| --- | --- |
| `projects/UM/` | Executável Windows, painel, entrada do mouse e desenho SDL. |
| `cs2/shared/` | Inicialização do CS2, leitura de dados, mira, trigger e sobreposição. |
| `library/vm/` | Camada de acesso a processos e memória usada pelos projetos. |
| `tests/` | Testes de regressão com dados simulados. Veja [como executar](tests/README.md). |
| `docs/` | [Verificação de builds](docs/BUILD_CHECK.md) e [estado das correções](docs/FIXES_STATUS.md). |
