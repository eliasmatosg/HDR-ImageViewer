#include <math.h>
#include <string.h> // para usar strings

// Rotinas para acesso da OpenGL
#include "opengl.h"

// Protótipos
void process();
void carregaHeader(FILE *fp);
void carregaImagem(FILE *fp, int largura, int altura);
void criaImagensTeste();
void conversion(float *floatBit, unsigned char conversionFactor);
void exposureApply(float *floatBit, float exposure);
void toneMapping(float *floatBit);
void gamaCorrection(float *floatBit);
void bitConversion(float *floatBit);

//
// Variáveis globais a serem utilizadas (NÃO ALTERAR!)
//

// Dimensões da imagem de entrada
int sizeX, sizeY;

// Pixels da imagem de ENTRADA (em formato RGBE)
RGBE *image;

// Pixels da imagem de SAÍDA (em formato RGB)
RGBunsigned *image8;

// Fator de exposição
float exposure;

// Histogramas
float histogram[HISTSIZE];
float adjusted[HISTSIZE];

// Flag para exibir/ocultar histogramas
unsigned char showhist = 0;

// Níveis mínimo/máximo (preto/branco)
int minLevel = 0;
int maxLevel = 255;

// Struct IHDR
IHDR ihdr;

void conversion(float *floatBit, unsigned char conversionFactor) {
  float c = pow(2, (conversionFactor - 136));
  *floatBit *= c;
}

void exposureApply(float *floatBit, float expos) { *floatBit *= expos; }

void toneMapping(float *floatBit) {
  *floatBit *= 0.6;
  *floatBit = ((*floatBit) * (2.51 * (*floatBit) + 0.03)) /
              ((*floatBit) * (2.43 * (*floatBit) + 0.59) + 0.14);
  if (*floatBit > 1)
    *floatBit = 1;
  if (*floatBit < 0)
    *floatBit = 0;
}

void gamaCorrection(float *floatBit) { *floatBit = pow(*floatBit, (1 / 2.2)); }

void bitConversion(float *floatBit) { *floatBit = *floatBit * 255.0; }

// Função principal de processamento: ela deve chamar outras funções
// quando for necessário (ex: algoritmo de tone mapping, etc)
void process() {
  printf("Exposure: %.3f\n", exposure);
  float expos = pow(2, exposure);

  RGBunsigned *ptr8 = image8;
  RGBE *ptr = image;
  int tamanho = sizeX * sizeY;

  float iHist = 0;
  float maxHist = 0;
  float iLevels = 0;
  float maxAdj = 0;

  float *fpixels = malloc(sizeX * sizeY * 3 * sizeof(float));

  int pixels = 0;
  for (int pos = 0; pos < tamanho; pos++) {

    fpixels[pixels] = ptr->r;
    fpixels[pixels + 1] = ptr->g;
    fpixels[pixels + 2] = ptr->b;

    conversion(&fpixels[pixels], ptr->e);
    conversion(&fpixels[pixels + 1], ptr->e);
    conversion(&fpixels[pixels + 2], ptr->e);
    exposureApply(&fpixels[pixels], expos);
    exposureApply(&fpixels[pixels + 1], expos);
    exposureApply(&fpixels[pixels + 2], expos);
    toneMapping(&fpixels[pixels]);
    toneMapping(&fpixels[pixels + 1]);
    toneMapping(&fpixels[pixels + 2]);
    gamaCorrection(&fpixels[pixels]);
    gamaCorrection(&fpixels[pixels + 1]);
    gamaCorrection(&fpixels[pixels + 2]);
    bitConversion(&fpixels[pixels]);
    bitConversion(&fpixels[pixels + 1]);
    bitConversion(&fpixels[pixels + 2]);

    iHist = (0.299 * fpixels[pixels]) + (0.587 * fpixels[pixels + 1]) + (0.114 * fpixels[pixels + 2]); // 255
    
    if (iHist >= 255.0) iHist = 255.0;
    else if (iHist < 0.0) iHist = 0.0;
    
    histogram[(int)iHist]++;
    if (histogram[(int)iHist] > maxHist)
      maxHist = histogram[(int)iHist];

    iLevels =
        fmin(1.0, ((fmax(0.0, iHist - minLevel)) / (maxLevel - minLevel))) * 255.0;

    int redHist   = (int)(fpixels[pixels]     * iLevels) / iHist;
    int greenHist = (int)(fpixels[pixels + 1] * iLevels) / iHist;
    int blueHist  = (int)(fpixels[pixels + 2] * iLevels) / iHist;

    if (redHist > 255)
      redHist = 255;
    if (redHist < 0)
      redHist = 0;
    if (greenHist > 255)
      greenHist = 255;
    if (greenHist < 0)
      greenHist = 0;
    if (blueHist > 255)
      blueHist = 255;
    if (blueHist < 0)
      blueHist = 0;

    ptr8->r = (unsigned char)redHist;
    ptr8->g = (unsigned char)greenHist;
    ptr8->b = (unsigned char)blueHist;

    if (iLevels > 255.0) iLevels = 255.0;
    else if (iLevels < 0.0) iLevels = 0.0;

    adjusted[(int)iLevels]++;
    if (adjusted[(int)iLevels] > maxAdj)
      maxAdj = adjusted[(int)iLevels];

    ptr8++;
    ptr++;
    pixels += 3;
  }
  free(fpixels);

  for (int i = 0; i < HISTSIZE; i++) {

    histogram[i] = (histogram[i] / maxHist);
    adjusted[i] = (adjusted[i] / maxAdj);
  }

  buildTex();
}

int main(int argc, char **argv) {

  for (int i = 0; i < HISTSIZE; i++) {
    histogram[i] = 0;
    adjusted[i] = 0;
  }

  if (argc == 1) {
    printf("hdrvis [image file.hdf]\n");
    exit(1);
  }

  // Inicialização da janela gráfica
  init(argc, argv);

  //
  // PASSO 1: Leitura da imagem
  // A leitura do ihdr já foi feita abaixo
  //
  FILE *arq = fopen(argv[1], "rb");
  carregaHeader(arq);

  //
  // IMPLEMENTE AQUI o código necessário para
  // extrair as informações de largura e altura do ihdr
  //

  sizeX = ihdr.imgWidth[0] + (ihdr.imgWidth[1] * 256) +
          (ihdr.imgWidth[2] * 65536) +
          (ihdr.imgWidth[3] * 16777216); // largura = 1280
  sizeY = ihdr.imgHeight[0] + (ihdr.imgHeight[1] * 256) +
          (ihdr.imgHeight[2] * 65536) +
          (ihdr.imgHeight[3] * 16777216); // altura = 1024

  printf("\n%d x %d\n", sizeX, sizeY);

  // Descomente a linha abaixo APENAS quando isso estiver funcionando!
  //
  carregaImagem(arq, sizeX, sizeY);

  // Fecha o arquivo
  fclose(arq);

  //
  // COMENTE a linha abaixo quando a leitura estiver funcionando!
  // (caso contrário, ele irá sobrepor a imagem carregada com a imagem de teste)
  //
  // criaImagensTeste();

  exposure = 0.0f; // exposição inicial

  // Aplica processamento inicial
  process();

  // Não retorna... a partir daqui, interação via teclado e mouse apenas, na ja
  // ela gráfica

  // Mouse wheel é usada para aproximar/afastar
  // Setas esquerda/direita: reduzir/aumentar o fator de exposição
  // A/S: reduzir/aumentar o nível mínimo (black point)
  // K/L: reduzir/aumentar o nível máximo (white point)
  // H: exibir/ocultar o histograma
  // ESC: finalizar o programa

  glutMainLoop();

  return 0;
}

// Função apenas para a criação de uma imagem em memória, com o objetivo
// de testar a funcionalidade de exibição e controle de exposição do programa
void criaImagensTeste() {
  // TESTE: cria uma imagem de 800x600
  sizeX = 800;
  sizeY = 600;

  printf("%d x %d\n", sizeX, sizeY);

  // Aloca imagem de entrada (32 bits RGBE)
  image = (RGBE *)malloc(sizeof(RGBE) * sizeX * sizeY);

  // Aloca memória para imagem de saída (24 bits RGB)
  image8 = (RGBunsigned *)malloc(sizeof(RGBunsigned) * sizeX * sizeY);
}

// Esta função deverá ser utilizada para ler o conteúdo do ihdr
// para a variável ihdr (depois você precisa extrair a largura e altura da i
// agem desse vetor)
void carregaHeader(FILE *fp) {
  // Lê 11 bytes do início do arquivo
  fread(&ihdr, sizeof(ihdr), 1, fp);
  // Exibe os 3 primeiros caracteres, para verificar se a leitura ocorreu
  // corretamente
  printf("Id: %c%c%c\n", ihdr.id[0], ihdr.id[1], ihdr.id[2]);
}

// Esta função deverá ser utilizada para carregar o restante
// da imagem (após ler o ihdr e extrair a largura e altura corretamente)
void carregaImagem(FILE *fp, int largura, int altura) {
  sizeX = largura;
  sizeY = altura;

  // Aloca imagem de entrada (32 bits RGBE)
  image = (RGBE *)malloc(sizeof(RGBE) * sizeX * sizeY);

  // Aloca memória para imagem de saída (24 bits RGB)
  image8 = (RGBunsigned *)malloc(sizeof(RGBunsigned) * sizeX * sizeY);

  // Lê o restante da imagem de entrada
  fread(image, sizeof(RGBE) * sizeX * sizeY, 1, fp);
  // Exibe primeiros 3 pixels, para verificação
  RGBE *ptr = image;
  int tamanho = sizeX * sizeY;
  for (int pos = 0; pos < 4; pos++) {
    printf("%4d %4d %4d %4d\n", ptr->r, ptr->g, ptr->b, ptr->e);
    ptr++;
  }
}
