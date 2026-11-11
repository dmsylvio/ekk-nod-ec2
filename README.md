# EKK NOD EC2

Projeto em C++ para estudar ferramentas externas de Counter-Strike 2 em partidas locais com bots. O executável **UM** roda em modo usuário no Windows e reúne uma sobreposição SDL, mira automática e trigger configuráveis.

O projeto também serve como base para estudar detecção de trapaças. Os controles documentados aqui são recursos do cliente de teste. O bloqueio de dados por distância no servidor continua sendo uma ideia de pesquisa.

## Requisitos

- Windows x64.
- Visual Studio 2022 com **Desktop development with C++**, toolset **MSVC v143** e Windows SDK.
- Counter-Strike 2 com a build interna **14196** (Steam build **26445667**). O código verifica a build antes de usar a matriz de projeção. Atualizações do jogo podem exigir ajustes.
- A `SDL3.dll` incluída em `projects/UM`, ao lado do executável durante a execução.

## Compilar e executar

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

O [workflow Linux x64](https://github.com/dmsylvio/ekk-nod-ec2/actions/workflows/linux-x64.yml) compila e empacota o executável no GitHub Actions. Abra a execução bem-sucedida mais recente e baixe **ekk-linux-x64** em **Artifacts**. O pacote inclui a biblioteca SDL3 necessária.

Depois de extrair o ZIP baixado do GitHub, execute:

```sh
tar -xzf ekk-linux-x64.tar.gz
cd ekk-linux-x64
./ekk-linux
```

A compilação e a checagem das dependências passaram no Ubuntu 22.04. O funcionamento em jogo ainda precisa ser validado. O painel de configuração Win32 é exclusivo da build Windows.

## Estrutura do repositório

| Pasta | Conteúdo |
| --- | --- |
| `projects/UM/` | Executável Windows, painel, entrada do mouse e desenho SDL. |
| `cs2/shared/` | Inicialização do CS2, leitura de dados, mira, trigger e sobreposição. |
| `library/vm/` | Camada de acesso a processos e memória usada pelos projetos. |
| `tests/` | Testes de regressão com dados simulados. Veja [como executar](tests/README.md). |
| `docs/` | [Verificação de builds](docs/BUILD_CHECK.md) e [estado das correções](docs/FIXES_STATUS.md). |
