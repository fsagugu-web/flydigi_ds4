# flydigi_ds4

**Use o seu controle Flydigi Direwolf 2 (USB) como se fosse um DualShock 4 nos jogos do PS4 desbloqueado.**

Plugin para o [GoldHEN](https://github.com/GoldHEN/GoldHEN) que lê o controle pela porta USB e entrega os comandos ao jogo no formato do DS4. Funciona **com o DS4 desligado**, não precisa de adaptador e é instalado com o FileZilla em poucos minutos.

> *English summary at the end of this page.*

---

## Por que isso existe

O PS4 só aceita controles que se identificam como DualShock 4 e passam numa verificação criptográfica da Sony. Controles para PC (Flydigi, genéricos, 8BitDo…) falam "a língua do Xbox" e são simplesmente ignorados. A solução comum é comprar um adaptador.

Este plugin resolve de outro jeito: ele roda **dentro do jogo**, lê o controle direto pela USB e entrega os botões ao jogo, que acha que está falando com um DS4. Como o sistema do PS4 nunca chega a ver o controle, não existe verificação de chave nem o limite de 8 minutos dos controles não licenciados.

## Recursos

- ✅ Todos os botões, analógicos e gatilhos analógicos
- ✅ Vibração
- ✅ Funciona com o DS4 desligado ou junto com ele (os dois ao mesmo tempo)
- ✅ Conectar e desconectar o cabo com o jogo aberto
- ✅ Arquivo de configuração: remapear botões (inclusive combinações), zona morta, gatilhos, vibração
- ✅ Configuração diferente por jogo
- ✅ Touchpad: clique, ou deslizar o dedo usando o analógico direito
- 🧪 Giroscópio (experimental) para jogos com mira/câmera por movimento
- ✅ Avisos na tela (conectado, desconectado, erro no arquivo de configuração)

## Compatibilidade

| | Situação |
|---|---|
| **Console** | PS4 desbloqueado com GoldHEN 2.4 ou mais novo (com o carregador de plugins) |
| **Testado em** | Firmware 12.52 + GoldHEN v2.4b18.7 |
| **Flydigi Direwolf 2** (cabo USB) | ✅ Testado |
| **Outros controles em modo XInput** (genéricos, 8BitDo, GameSir…) | Devem funcionar, mas não foram testados. [Conte como foi](#seu-controle-não-funcionou) |
| **Controles de Xbox One / Series** | ❌ Não suportado (protocolo diferente) |
| **Bluetooth / receptor sem fio** | ❌ Só cabo USB (o Bluetooth é controlado pelo sistema) |
| **Jogos testados** | CUSA17776. Testou outro? Abra uma *issue* contando! |

## Instalação

Você precisa do FileZilla (ou outro cliente FTP) e do GoldHEN com o servidor FTP ligado (porta **2121**).

1. Baixe o arquivo `flydigi_ds4-v1.3.zip` na página de [Releases](../../releases) e extraia.
2. No FileZilla, conecte no IP do PS4, porta `2121`.
3. Envie **`flydigi_ds4.prx`** para `/data/GoldHEN/plugins/`
4. Envie **`flydigi_ds4.ini`** para `/data/GoldHEN/`
5. Adicione o plugin ao arquivo `/data/GoldHEN/plugins.ini`:
   - Clique com o botão direito em `plugins.ini` → **Exibir/Editar**
   - Embaixo da linha `[default]`, adicione:
     ```
     /data/GoldHEN/plugins/flydigi_ds4.prx
     ```
   - Salve, feche e confirme o envio de volta. *(Se o arquivo não existir, envie o `plugins.ini` que vem no zip.)*
6. No menu do GoldHEN, confirme que **Enable Plugins Loader** está ligado.
7. Ligue o Flydigi no cabo USB e abra um jogo. Alguns segundos depois aparece **"Flydigi conectado"**.

Para desinstalar, apague a linha do `plugins.ini`.

## Mapa de botões padrão

| Flydigi | PS4 |
|---|---|
| A / B / X / Y | ✕ / ○ / □ / △ |
| LB / RB | L1 / R1 |
| LT / RT | L2 / R2 (analógicos) |
| Menu (Start) | Options |
| View (Back) | Clique do touchpad |
| L3 / R3, analógicos, direcional | iguais |
| Botão Flydigi/Home | nada (o botão PS é do sistema) |

## Configuração

Tudo fica em `/data/GoldHEN/flydigi_ds4.ini`, que você edita pelo FileZilla (botão direito → **Exibir/Editar**). As mudanças valem na próxima vez que abrir um jogo. Se escrever algo errado, aparece um aviso na tela dizendo a linha.

| Opção | Valores | Padrão | O que faz |
|---|---|---|---|
| `A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `START`, `BACK`, `GUIDE`, `L3`, `R3`, `UP`, `DOWN`, `LEFT`, `RIGHT` | `CROSS` (`XIS`), `CIRCLE` (`BOLA`), `SQUARE` (`QUADRADO`), `TRIANGLE` (`TRIANGULO`), `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `OPTIONS`, `TOUCHPAD`, `UP`, `DOWN`, `LEFT`, `RIGHT`, `NONE` | tabela acima | Remapeia o botão. Aceita combinação: `LB = L1+R1` |
| `deadzone_left`, `deadzone_right` | 0 a 50 (%) | 9 | Zona morta dos analógicos |
| `trigger_threshold` | 1 a 100 (%) | 8 | Quanto apertar LT/RT para contar como apertado |
| `vibration` | 0 a 100 (%) | 100 | Intensidade da vibração |
| `touchpad_mode` | `click` ou `swipe` | `click` | Veja [Touchpad](#touchpad) |
| `gyro` | `sim` / `nao` | `nao` | Veja [Giroscópio](#giroscópio-experimental) |
| `gyro_sensitivity` | 1 a 500 (%) | 100 | Sensibilidade do giroscópio |
| `gyro_invert_horizontal`, `gyro_invert_vertical` | `sim` / `nao` | `nao` | Inverte um eixo do giroscópio |
| `notifications` | `sim` / `nao` | `sim` | Avisos de conectado/desconectado |
| `log` | `sim` / `nao` | `nao` | Grava `/data/GoldHEN/flydigi_ds4.txt` para diagnóstico |

**Configuração por jogo:** crie uma seção com o código do jogo e coloque só o que muda nele:

```ini
[default]
vibration = 100

[CUSA17776]
vibration = 0
A = CIRCLE
B = CROSS
```

### Touchpad

- `touchpad_mode = click`: o View clica no touchpad.
- `touchpad_mode = swipe`: segurando o View, o **analógico direito vira o dedo** no touchpad, para deslizar.
  - Toque rápido no View: clique
  - Segurar o View parado por meio segundo: clique segurado
  - R3 enquanto segura o View: clique no ponto onde o dedo está
  - Soltando o View, o analógico volta a controlar a câmera

### Giroscópio (experimental)

Com `gyro = sim`, o movimento do Direwolf 2 vai para o giroscópio do DS4, para jogos com mira ou câmera por movimento.

No modo USB o Direwolf 2 só informa o movimento **horizontal e vertical**, já processado. Não tem inclinação nem acelerômetro, então jogos que usam o controle como volante ou nível não vão funcionar. A direção e a escala foram calibradas com testes de movimento, mas podem variar de jogo para jogo: se a mira andar ao contrário, use `gyro_invert_horizontal` / `gyro_invert_vertical`, e ajuste a força com `gyro_sensitivity`.

## Limitações

- **Botão PS e Share** continuam só no DS4: são do sistema, não do jogo.
- **Menu do PS4** (fora dos jogos) continua precisando do DS4 ou do app PS4 Second Screen no celular.
- **Um controle só**: não existe jogador 2 pelo plugin, porque o PS4 só cria o jogador 2 quando outro usuário entra com um controle reconhecido pelo sistema.
- Não usar junto com o plugin `gamepad_helper` (os dois mexem na mesma parte do jogo).

## Problemas comuns

**O controle conecta e desconecta sozinho**
Troque o cabo USB. Cabo ruim foi exatamente o que causou isso nos testes.

**Não aparece "Flydigi conectado"**
Confirme que o `Enable Plugins Loader` está ligado, que a linha está no `plugins.ini` e que o controle foi ligado no cabo antes ou durante o jogo. Ligue `log = sim` e veja o arquivo `/data/GoldHEN/flydigi_ds4.txt`.

**O jogo fecha ao abrir**
Apague a linha do `plugins.ini` e abra uma *issue* com o código do jogo (CUSAxxxxx) e o log.

**O controle continua vibrando depois de fechar o jogo**
Tire e recoloque o cabo.

## Seu controle não funcionou?

Dá para adicionar suporte sem eu ter o controle:

1. No `flydigi_ds4.ini`, coloque `log = sim`
2. Ligue o controle e abra um jogo
3. Baixe `/data/GoldHEN/flydigi_ds4.txt` e anexe numa [issue](../../issues/new/choose)

O log descreve tudo que estiver ligado na USB, que é o que eu preciso para entender o controle.

## Compilando (Windows)

```powershell
powershell -ExecutionPolicy Bypass -File setup.ps1          # baixa o toolchain (~350 MB)
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 # testes no PC
powershell -ExecutionPolicy Bypass -File build.ps1           # gera out\flydigi_ds4.prx
```

Usa [llvm-mingw](https://github.com/mstorsjo/llvm-mingw), o [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) e o [GoldHEN Plugins SDK](https://github.com/GoldHEN/GoldHEN_Plugins_SDK), em versões fixas baixadas pelo `setup.ps1`.

### Como funciona por dentro

| Arquivo | Função |
|---|---|
| `source/main.c` | Intercepta `scePadRead`/`scePadReadState` (mesma técnica do plugin oficial `gamepad_helper`) e mistura o Flydigi aos dados do DS4; repassa a vibração |
| `source/usb.c` | Thread que encontra o controle (qualquer interface XInput `FF/5D/01`), lê os relatórios pela `libSceUsbd` e reconecta sozinha |
| `source/xinput.c` | Relatório USB do Xbox 360 → estado neutro (`RawPad`), incluindo os bytes de movimento do Direwolf 2 |
| `source/translate.c` | `RawPad` → DS4, aplicando a configuração e o touchpad |
| `source/gyro.c` | Movimento → velocidade angular do giroscópio do DS4 |
| `source/config.c` | Leitor do `flydigi_ds4.ini` |
| `tests/tests.c` | Testes no PC, incluindo relatórios e movimentos reais capturados do Direwolf 2 |

## Créditos

- [GoldHEN](https://github.com/GoldHEN) — SDK de plugins e a técnica de interceptar o controle do plugin `gamepad_helper` (por [jocover](https://github.com/jocover))
- [OpenOrbis](https://github.com/OpenOrbis) — toolchain de homebrew para PS4
- [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) — compilador

Detalhes das licenças em [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Licença e aviso

Código aberto sob a licença **GPL-3.0** ([LICENSE](LICENSE)). Você pode usar, estudar, modificar e redistribuir, desde que mantenha o código aberto sob a mesma licença.

Projeto independente, **sem relação com Sony, Flydigi ou Microsoft**. PlayStation, DualShock e PS4 são marcas da Sony Interactive Entertainment; Flydigi e Xbox pertencem aos seus donos. Use em consoles que são seus, por sua conta e risco. Este projeto não tem nenhuma relação com pirataria e não deve ser usado para isso.

---

## ☕ Apoie o projeto

O flydigi_ds4 é e vai continuar sendo **gratuito e de código aberto**. Ele nasceu de uma frustração simples: um controle bom, que eu já tinha, que o PS4 se recusava a reconhecer. Foram muitas horas de engenharia reversa, testes no console e ajustes até chegar aqui.

Se o plugin te poupou a compra de um adaptador, ou simplesmente fez você voltar a jogar com o controle que você gosta, considere fazer uma doação de qualquer valor. Ela ajuda a manter o projeto vivo, a comprar outros controles para testar e a responder as *issues* com calma.

<table>
  <tr>
    <td align="center">
      <b>PIX</b><br><br>
      <!-- Opcional: gere o QR Code no app do banco, salve como docs/pix-qrcode.png e apague a linha de comentário abaixo -->
      <!-- <img src="docs/pix-qrcode.png" width="180" alt="QR Code PIX"><br><br> -->
      Chave aleatória:<br>
      <code>b5a43469-bdb5-417d-9d75-68f3427b6f67</code>
    </td>
  </tr>
</table>

Não pode doar? Tudo bem! Deixar uma ⭐ no repositório, contar em quais jogos funcionou ou indicar para um amigo também ajuda muito. **Obrigado!** 💙

---

## English summary

**flydigi_ds4** is a GoldHEN plugin that makes a **Flydigi Direwolf 2** (USB cable, XInput mode) work as a DualShock 4 in jailbroken PS4 games. No adapter, no auth bypass: it runs inside the game, reads the controller through `libSceUsbd` and merges it into what the game gets from `libScePad`. Works with the DS4 turned off.

Features: all buttons, sticks, analog triggers, rumble, hot-plug, remapping and per-game settings (`flydigi_ds4.ini`), touchpad click/swipe, experimental gyro. Tested on FW 12.52 + GoldHEN v2.4b18.7. Other XInput controllers should work but are untested; Xbox One/Series pads are not supported.

Install: copy `flydigi_ds4.prx` to `/data/GoldHEN/plugins/`, `flydigi_ds4.ini` to `/data/GoldHEN/`, and add `/data/GoldHEN/plugins/flydigi_ds4.prx` under `[default]` in `/data/GoldHEN/plugins.ini`. Licensed under GPL-3.0. Not affiliated with Sony, Flydigi or Microsoft.
