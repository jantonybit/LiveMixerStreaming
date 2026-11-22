# Política de Privacidade — LiveMixer Streaming

**Vigente desde:** 22 de novembro de 2026 · **Versão:** 1.0  
**Publicador:** Jantonybit · **Identidade da Store:** `Jantonybit.LiveMixerStreaming`

---

## Resumo

O **LiveMixer Streaming** é um aplicativo de áudio que roda **inteiramente no seu computador**.
Ele **não cria conta, não coleta dados pessoais, não envia telemetria, não exibe anúncios e
não grava nem transmite o seu áudio para lugar algum**. O áudio é processado em memória e
entregue às saídas que você escolher no próprio app.

## 1. Quem é o responsável

O aplicativo é publicado por **Jantonybit**. Esta política explica o que acontece com as
informações quando você usa o LiveMixer Streaming. Em caso de dúvidas, use a aba de
`Issues` do repositório do projeto no GitHub (endereço indicado na página da Store).

## 2. Dados que coletamos

**Nenhum.** Não há:

- cadastro ou login;
- telemetria, analytics, relatórios de falhas ou contadores de uso;
- cookies, identificadores de publicidade ou SDKs de terceiros;
- acesso a contatos, arquivos pessoais, localização, câmera ou galeria;
- transferência de áudio, texto ou qualquer outro conteúdo para servidores.

O aplicativo é **gratuito**, não contém compras dentro do app (IAP) e não pede doações.

## 3. Áudio e dispositivos (permissão de microfone)

Para funcionar como mesa de som, o app solicita acesso a dispositivos de áudio:

| Acesso | Para que serve | Para onde vai o áudio |
|---|---|---|
| Entradas ASIO / WASAPI (microfones, interface de áudio) | Receber os sinais das faixas do mixer | Processado em memória; sai pelas saídas ASIO e/ou **Master Out** escolhidas por você |
| Loopback de reprodução WASAPI ("Audio do PC") | Capturar o som que o Windows está enviando a um dispositivo | Mesmo processamento local; não é salvo em disco |
| Captura por aplicativo (`Process Loopback`) | Capturar separadamente o áudio de um programa que esteja tocando | Mesmo processamento local; apps de streaming e o próprio LiveMixer são ignorados para evitar realimentação |
| Dispositivos MIDI | Controlar parâmetros do mixer com controladores físicos | Apenas comandos locais; nada é enviado pela rede |

O áudio **não** é arquivado, codificado para upload, nem usado para treinar modelos. As
opções **"Nao ouvir no PC"** e **"Desviar apps do Windows"** apenas silenciam temporariamente
dispositivos/sessões do Windows enquanto o motor está ativo e **restauram o estado original**
ao parar ou ao fechar o app.

> Para usar o modo de baixa latência é necessário ter um **driver ASIO instalado pelo
> fabricante da sua interface**. O LiveMixer Streaming não instala drivers e não redistribui
> drivers de terceiros.

## 4. Dados salvos no seu computador

O app salva apenas configurações de uso, em arquivos de texto na pasta da aplicação (quando
instalado via MSIX, no armazenamento local virtualizado do pacote):

- `config\ui.cfg` — idioma, tema e microfone preferido;
- `config\mixer.cfg` — níveis, efeitos, rotas e envios das faixas;
- `config\midi.map` — mapeamento de MIDI Learn;
- `config\promos.cfg` — links de cross-promotion;
- `config\scenes\*` — cenas criadas por você.

Esses arquivos **nunca são enviados**. Para removê-los, desinstale o aplicativo ou apague a
pasta `config` da instalação.

## 5. Internet e links

O aplicativo **não faz requisições de rede por conta própria**. A única ação relacionada à
internet é abrir, mediante **clique explícito seu**, páginas da Microsoft Store (links de
outros apps do publicador) no aplicativo da Store ou no navegador padrão do Windows. Esses
links estão listados no arquivo `config\promos.cfg` e podem ser apagados por você. Não há
pop-ups automáticos nem abertura de links sem interação.

## 6. Compartilhamento com terceiros

Não compartilhamos dados com terceiros. Não há redes de anúncios, serviços de analytics,
serviços de nuvem nem processadores de pagamento dentro do app. A Microsoft pode tratar
informações referentes à **sua conta da Microsoft e à instalação** conforme a política da
Microsoft Store, pois a distribuição é feita por lá.

## 7. Crianças

O aplicativo não é direcionado a menores de 13 anos e não coleta informações de crianças.

## 8. Alterações

Se esta política mudar, a nova versão será publicada neste mesmo endereço com a data
atualizada no topo.

## 9. Contato

Dúvidas ou solicitações relacionadas à privacidade devem ser registradas como `Issue` no
repositório do LiveMixer Streaming no GitHub (link disponível na página da Store).

---

*LiveMixer Streaming · Sound Designer by Jantonybit · Política de Privacidade v1.0*
