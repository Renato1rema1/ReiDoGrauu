# Rei do Grau — Unreal Engine 5

Jogo de grau de moto em mundo aberto, na orla molhada de Porto Brasa ao pôr do sol.
É a versão Unreal Engine 5 do jogo web, com a mesma física de grau, a mesma cidade e os mesmos desafios.
A diferença é a imagem: luz global e reflexos em tempo real (Lumen), nuvens volumétricas, neblina com luz e motion blur.

O projeto funciona sem nenhum modelo 3D baixado: a moto, o piloto, a cidade, o trânsito e o céu são gerados por código.
Para chegar perto do vídeo de referência, troque as formas provisórias por modelos realistas (veja a seção 5).

## 1. O que você precisa

| Item | Mínimo | Recomendado |
|---|---|---|
| Placa de vídeo | DirectX 12, 6 GB (GTX 1660 / RX 5600) | RTX 3060 ou melhor (ray tracing) |
| Memória RAM | 16 GB | 32 GB |
| Espaço livre | 150 GB | 200 GB em SSD |
| Sistema | Windows 10/11 64 bits | Windows 11 |

Com placa integrada (Intel Iris Xe, Radeon Vega) a Unreal 5 não roda bem. Uma alternativa é alugar um PC na nuvem com placa RTX (Shadow PC, ou uma máquina com GPU na AWS ou Azure).

## 2. Instalação (uma vez só)

1. Instale o **Epic Games Launcher** (epicgames.com) e, na aba Unreal Engine > Biblioteca, instale a **Unreal Engine 5.5** (ou mais nova).
2. Instale o **Visual Studio 2022 Community** com a carga de trabalho **"Desenvolvimento de jogos com C++"**. Na lista da direita, marque também o **Windows SDK** e o **.NET** sugeridos pela Epic para a sua versão da engine.
3. Copie a pasta `ReiDoGrauUE` para um lugar fixo, por exemplo `D:\Projetos\ReiDoGrauUE` (evite caminhos muito longos).

## 3. Primeira abertura

1. Dê dois cliques em `ReiDoGrau.uproject`.
2. A engine vai dizer que faltam os módulos `ReiDoGrau` e perguntar se quer compilar agora. Clique em **Sim**.
   A primeira compilação demora alguns minutos.
   - Se aparecer erro, clique com o botão direito no `.uproject` > **Generate Visual Studio project files**, abra o `ReiDoGrau.sln`, escolha a configuração **Development Editor** e mande compilar (Ctrl+Shift+B). Os erros aparecem na janela de saída; me mande o texto que eu corrijo.
   - Se a sua engine for mais nova que a 5.5, ela pergunta se quer converter o projeto: escolha **Abrir uma cópia** ou **Converter no lugar**.
3. Com o editor aberto, vá em **Tools > Execute Python Script...** e escolha `Content/Python/setup_portobrasa.py`.
   Ele cria os materiais realistas (asfalto molhado com poças, calçadão de ondas, fachadas com janelas acesas, pintura com verniz, mar) e o mapa `L_PortoBrasa`.
4. Aperte **Play** (Alt+P). Na primeira vez a engine compila shaders, então os primeiros minutos podem ficar lentos.

Mesmo sem rodar o script, o Play já funciona em qualquer mapa vazio: o jogo cria o pôr do sol, a cidade e o trânsito sozinho, só que com materiais simples.

## 4. Controles

| Tecla | Ação |
|---|---|
| W / ↑ | Acelerar |
| S / ↓ | Frear (no grau, o freio traseiro abaixa a frente) |
| A D / ← → | Inclinar e fazer curva |
| Shift | Puxar o grau (peso pra trás + embreagem) |
| Ctrl / X | Peso pra frente (com freio forte faz stoppie) |
| W + S parado | Queimar pneu; com A ou D faz zerinho |
| E | Começar o desafio da coluna laranja |
| C | Trocar câmera (perseguição, próxima, cinema, capacete) |
| R | Voltar pra pista |
| M | Som liga/desliga |
| P | Pausar |
| Mouse | Olhar em volta |
| Controle | RT acelera, LT freia, analógico vira, A puxa o grau, B peso pra frente, Y câmera, X desafio, Start pausa |

Dicas: em 1ª ou 2ª marcha, segure W e aperte Shift. No grau, a faixa verde do marcador é o ponto de equilíbrio.
Acima de 74° a rabeta raspa no asfalto e solta faísca (vale ponto extra).
Passar raspando nos carros acima de 55 km/h dá "tira fina".

Desafios: Orla em Grau, Grau no 12, Rolê até o Posto, Freio de Mão e Rei da Quebrada.
O dinheiro, os pontos e os desafios concluídos ficam salvos.

## 5. Deixando realista (modelos de verdade)

A Unreal fica realista com bons modelos 3D. Todos os itens abaixo são encaixados sem mexer em código, pelo painel **Details** do editor.

**Onde pegar (grátis ou baratos):**
- **Fab** (fab.com, loja da Epic): cenários do Quixel Megascans grátis para Unreal (rochas, vegetação, calçadas, asfalto), coqueiros, postes e carros.
- **City Sample** (grátis, da Epic): prédios e carros fotorrealistas da demo Matrix Awakens.
- **Moto**: procure "sport bike", "superbike" ou "motorcycle" no Fab ou no Sketchfab (confira a licença). Prefira um modelo com rodas separadas.
- **Piloto**: o Manny do template Third Person (grátis) ou personagens do MetaHuman, com uma animação de piloto sentado (Fab ou Mixamo).

**Onde encaixar:**
- Selecione o ator **Porto Brasa** no mapa e preencha `Grau|Modelos`: `PalmMeshes` (coqueiros), `TreeMeshes`, `StreetLampMesh`.
- Selecione o ator **Transito** e preencha `CarMeshes` (frente do carro para +X, origem no chão).
- Para a moto, crie um Blueprint filho de `GrauBike` (botão direito em Content > Blueprint Class > procure GrauBike), preencha `BikeMesh` e ajuste `BikeMeshOffset` até a roda traseira encostar na origem. Para o piloto, `RiderMesh` + `RiderPose`. Com um piloto de esqueleto, o tombo vira ragdoll de verdade.
  Depois crie um Blueprint filho de `GrauGameMode`, coloque a sua moto em **Default Pawn Class** e escolha esse Game Mode em **Edit > Project Settings > Maps & Modes > Default GameMode**.
- O sol, a neblina e a cor da imagem ficam no ator **Por do Sol** (`SunElevation` entre 3 e 7 dá o laranja do vídeo).

## 6. Como o código está organizado

| Arquivo | O que faz |
|---|---|
| `GrauBike` | A moto: motor, câmbio, inclinação, grau, stoppie, rabeta raspando, saltos, colisão, tombo, câmeras |
| `GrauEngineSynth` | Ronco do 4 cilindros sintetizado em tempo real (não precisa de arquivo de som) |
| `GrauScoreComponent` | Pontos, combos, tira fina, recordes |
| `GrauCityGenerator` | A cidade inteira por instâncias: ruas, faixas, prédios, postes, coqueiros, posto, pista, praça, morros, comunidade, mar |
| `GrauTraffic` | Carros seguindo as faixas e reagindo à moto |
| `GrauWorldSetup` | Pôr do sol: sol, atmosfera, nuvens volumétricas, neblina, céu em tempo real, pós-processamento |
| `GrauGameMode` | Monta o mundo se faltar algo, desafios e salvamento |
| `GrauHUD` | Velocímetro, marcador do grau, dinheiro, mensagens e minimapa |
| `Content/Python/setup_portobrasa.py` | Cria materiais e o mapa dentro do editor |

A física da moto trabalha em metros e segundos, com passos fixos de 1/120 s. Os números para ajustar ficam em `GrauBike` na categoria `Grau|Fisica` (massa, potência, impacto de tombo, assistência de grau).

## 7. Observações

- Este projeto foi escrito fora do editor e não foi compilado na máquina onde foi feito. Se a compilação acusar algum erro, mande a mensagem completa.
- A primeira abertura de qualquer projeto UE5 é lenta por causa da compilação de shaders.
- Para gerar o jogo instalável: **Platforms > Windows > Package Project**.
