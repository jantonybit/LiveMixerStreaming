# HANDOFF — LiveMixer Streaming

Estado do projeto em **22/11/2026**. Este arquivo existe para permitir continuar o trabalho
num prompt/sessão nova sem perder decisões já tomadas.

## 1. Objetivo da próxima etapa

Finalizar a **documentação de distribuição** e gerar o pacote **MSIX** para publicação na
Microsoft Store, incluindo/README, privacy policy, metadados do listing, manifesto,
assinatura e validação com `MakeAppx`/`SignTool`.

## 2. Como compilar / onde está o executável

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

- Executável: `bin\CentralDeAudio.exe` (aprox. 5,57 MB, sem `strip`).
- Toolchain **portátil**, tudo dentro de `ThirdParty\`: MinGW-w64, CMake, Ninja, Dear ImGui e
  SDK ASIO. Nada é instalado no Windows (sem PATH, sem registro, sem pastas de sistema).
- Link **estático**: `-static -static-libgcc -static-libstdc++`. Não precisa das DLLs do
  MinGW ao lado do `.exe`.
- `scripts\build.ps1` usa nome curto 8.3 e um espelho de *hardlinks* do MinGW em
  `%LOCALAPPDATA%\Temp\opencode\central-audio-toolchain\mingw64` para contornar bug do
  `ld.exe` com espaços no caminho. Pode ser apagado; o script recria.
- Se o app estiver aberto, o linker falha com `Permission denied`:
  `Stop-Process -Name CentralDeAudio -Force` antes de compilar.
- `scripts\bootstrap.ps1` refaz o download das dependências se `ThirdParty` for apagada.

## 3. Estrutura relevante

```
src/
  main.cpp                 WinMain + COM
  app/App.cpp / App.h      estado, toolbar, toda a UI (inclui engrenagem)
  audio/AsioDevice.*       host ASIO real
  audio/WasapiEngine.*     loopback de reprodução + saída WASAPI
  audio/AppAudioCapture.*  captura por processo (ActivateAudioInterfaceAsync)
  audio/StereoRing.h       ring buffer SPSC sem alocação na thread de áudio
  dsp/                     Biquad, Fx (Delay/Reverb/Chorus), Meter, DspUtils
  mixer/Mixer.*            motor de mixagem
  midi/MidiEngine.*        MIDI Learn, latch, múltiplos controladores
  platform/AppWindow.*     Win32 + DX11, F11 fullscreen
  ui/Widgets.*             knobs, faders, medidores
docs/
  manual_pt.html
  manual_en.html
  HANDOFF.md               este arquivo
bin/
  CentralDeAudio.exe
  docs/                    cópia dos manuais (POST_BUILD do CMake)
  config/
    ui.cfg                 language, theme, microphone
    mixer.cfg
    midi.map
    promos.cfg             links de cross-promotion
    scenes\
ThirdParty/                toolchain + ImGui + asio_sdk
LICENSE.md                 GPLv3 + tabela de licenças de terceiros
README.md                  documentação principal (PT)
```

`ui.cfg` — chaves atuais: `language`, `theme`, `microphone`. As chaves antigas `knob.*`
foram **removidas**; cores dos knobs agora são fixas em código.
`promos.cfg` — formato `Nome|ProductId|Descrição curta` + `publisher=Jantonybit`.

## 4. Decisões já tomadas (não refazer)

### Produto / áudio
- **Master Out** é sempre um sink/clock WASAPI. Nunca usar o nome “OBS” na UI (a proteção
  contra realimentação ignora apps de streaming, mas o texto não deve citá-los).
- Modo ASIO = driver da interface como clock; modo WASAPI = Windows como clock (exige
  Master Out selecionado).
- Captura por app via `ActivateAudioInterfaceAsync` + `AUDIOCLIENT_ACTIVATION_PARAMS` +
  `VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK` (`L"VAD\\Process_Loopback"`), handler ágil com
  `IAgileObject` + `CoCreateFreeThreadedMarshaler`.
- `Desviar apps do Windows` = `ISimpleAudioVolume::SetMute` por sessão, estado restaurado
  ao parar.
- Rack de efeitos **entre canais e Master**, 3 módulos fixos:
  `aux_[0]=Delay`, `aux_[1]=Reverb`, `aux_[2]=Chorus`; `kNumAux=3`; `ChannelSend3` no fim de
  `MidiTargetType`. Envios **pós-fader**.
- Hard pan corrigido no DSP (`pan=-1` → `L≈0.6543, R≈0`, `pan=+1` → invertido).
- Apenas knob giratório de Pan. Raio: Ganho/Pan/EQ `15.0f * controlScale_`; envios auxiliares
  `13.0f * s`. `controlScale_ = clamp((stripHeight - 261)/325, 0.45, 1.50)`.
- F11 = `WS_POPUP` + `HWND_TOPMOST` nos limites do monitor; saída restaura
  `WS_OVERLAPPEDWINDOW` + `SetWindowPlacement`; repetição ignorada via bit `0x40000000u`.
- Largura do fader Master **não pode mudar**. Rack e Master com a mesma largura:
  `targetSide = clamp(availableWidth * 0.095f, 112.0f, 176.0f)`.
- Win10 build 20348+ / Win11, DX11. Driver ASIO é instalado pelo usuário; o app não instala
  drivers.

### Interface
- Idioma/tema persistidos em `bin\config\ui.cfg` via `g_language`/`tr()`.
- Temas: `Midnight`, `Gold`, `Ocean`, `Light`.
- Engrenagem: **fonte 14 pt** (18,67 px a 96 dpi) em todo o conteúdo; **títulos em negrito**
  na cor da marca (também o título da janela); janela 560×500 com padding 12 e mínimo
  proporcional.
- Cores dos knobs **não são mais personalizáveis** (sem `ColorEdit3`, sem `Restaurar cores`,
  sem `knob.*` no `ui.cfg`). Cores fixas por função: 🔴 Ganho, 🟡 EQ graves/agudos,
  🟠 EQ médios, 🔵 dinâmica, 🟣 FX, 🟢 envios/monitoramento, ⚪ neutro.
- Seção **Aplicativos do mesmo Segmento**: `DJMasterDeck|9PD722LB588M|Estação De audio
  Profissional para DJ` e `PlayLive|9NJ8XTJT12F6|Player de audio para apresentacoes ao vivo`.
- Seção **Outros Aplicativos do Desenvolvedor**: botão `Ver todos os meus apps` abre
  `ms-windows-store://publisher/?name=Jantonybit`, fallback `.../search/?query=Jantonybit`
  e depois `https://apps.microsoft.com/`. Links individuais usam
  `ms-windows-store://pdp/?ProductId=<id>`.
- Cross-promotion permitida apenas de forma discreta, honesta, por clique explícito, sem SDK
  de anúncios nem pop-up. **Decisão final: NÃO adicionar botão de doação dentro do app.**
  Se quiser, colocar link apenas em README/listing da Store.

### Distribuição / licença
- Distribuição **gratuita**.
- `LICENSE.md` e `README.md` já assumem **GPLv3** (opção open source do SDK ASIO). A
  Steinberg oferece também licença proprietária, que exige acordo assinado e envio para
  `sdklicenseagreements@steinberg.de`. **Confirmar qual caminho será usado antes de publicar.**
  - Se GPLv3: publicar o fonte completo correspondente e manter avisos de copyright.
  - Se proprietária: assinar acordo primeiro; aí o código pode ser fechado.
- Manter o aviso: *ASIO is a registered trademark of Steinberg Media Technologies GmbH*.
- Política Store **7.20** (vigente desde 22/10/2026), item **10.8.2**: doação voluntária pura
  deve usar Payment Request API ou processador terceirizado **seguro**; se houver qualquer
  bem digital em troca (recurso, selo, desbloqueio, remoção de anúncio), aí é obrigatório IAP
  da Store. Por isso o app não tem doação nem contrapartida digital.

## 5. Dados de identidade / Store

Fornecidos pelo usuário (origem: outro app do mesmo publicador — **confirmar se serão
reutilizados ou se este app terá identidade nova**):

- `Package/Identity/Name`: `Jantonybit.CNCCodeMaker`
- `Package/Identity/Publisher`: `CN=35FEBA35-4726-46E1-A569-8CAEEC82FBD7`
- `Package/Properties/PublisherDisplayName`: `Jantonybit`
- PFN: `Jantonybit.CNCCodeMaker_y87f9s8hjrx40`
- Store ID: `9ND862TKSWZZ`
- Deep link: `ms-windows-store://pdp/?productid=9ND862TKSWZZ`
- Web: `https://apps.microsoft.com/detail/9ND862TKSWZZ`

Referência oficial de URI: `https://learn.microsoft.com/en-us/windows/apps/develop/launch/launch-store-app`
Políticas: `https://learn.microsoft.com/en-us/windows/apps/publish/store-policies`
FAQ GPL (dinheiro/doação): `https://www.gnu.org/licenses/gpl-faq#DoesTheGPLAllowMoney`

## 6. Pendências para a etapa final

1. **Definir a licença ASIO final** (GPLv3 público vs. acordo proprietário Steinberg).
2. **Certificado de assinatura do MSIX** (Self-signed para teste local vs. certificado de
   publicação). `SignTool` faz parte do SDK do Windows; verificar se está disponível na
   máquina ou se precisa de toolchain portátil.
3. **Privacy policy** — ainda **não existe arquivo**. Precisa ser escrita e publicada em URL
   pública; o app não coleta dados financeiros nem telemetria, mas usa WASAPI/MIDI/Store.
4. **Metadados do listing**: título, descrição curta/longa, capturas, categoria, faixa etária,
   declaração de capacidades, aviso de driver ASIO externo.
5. **Manifesto MSIX**: identidade nova ou reaproveitada, `EntryPoint="Windows.FullTrustApplication"`,
   capacidades necessárias (`microphone` para entrada física; conferir `internetClient` para
   abertura de URIs da Store), assets (logo/tile) e `rescap`/`runFullTrust` se aplicável.
6. **Publicar o fonte** se confirmado GPLv3 (repo público ou oferta por escrito válida por 3 anos).
7. **Validação final**: `MakeAppx pack`, `SignTool sign`, instalação local, smoke launch,
   captura de tela/OCR da engrenagem (já validada nesta sessão: títulos, 14 pt, descrição do
   DJMasterDeck e botão do publicador apareceram corretamente).

## 7. Restrições / cuidados

- **Não há repositório git**; alterações não versionadas. Não assumir commits.
- Não usar `strip` no build.
- Não renomear Master Out para “OBS”/streaming em texto de UI.
- Não reintroduzir personalização de cores dos knobs.
- Não adicionar pop-up automático, anúncio, doação dentro do app nem desbloqueio por pagamento.
- `docs\manual_pt.html` / `manual_en.html` são copiados automaticamente para `bin\docs` a cada
  build; editar sempre os de `docs\`, nunca só a cópia.
- Arquivos em `.cache\` são auxiliares de teste (captura de tela, log de build, OCR) — **não**
  fazem parte da entrega.
- Se o app estiver rodando, `Stop-Process -Name CentralDeAudio -Force` antes de recompilar.
