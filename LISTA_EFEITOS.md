# Lista de Efeitos e Parâmetros

Este documento detalha todos os efeitos de iluminação disponíveis na luminária, a ordem em que aparecem e os parâmetros que você pode ajustar no "Modo de Edição de Efeito".

---

### 1. Candle (Vela)
**Descrição:** Simula uma chama de vela realista e bruxuleante, com variações naturais de brilho e cor.
| Parâmetro   | Descrição                                                              | Intervalo |
|-------------|------------------------------------------------------------------------|-----------|
| `Speed`     | Controla a velocidade do piscar da chama.                              | 1 - 50    |
| `Hue`       | Define a cor base da chama (geralmente tons de laranja/amarelo).       | 5 - 80    |
| `Saturation`| Controla a intensidade da cor da chama.                                | 0 - 255   |
| `Segments`  | Divide a fita de LED em seções que piscam de forma independente.       | 1 - 10    |

---

### 2. White Temp (Temperatura de Branco)
**Descrição:** Exibe uma cor branca sólida e estática.
| Parâmetro     | Descrição                                         | Intervalo |
|---------------|---------------------------------------------------|-----------|
| `Temperature` | Seleciona a tonalidade do branco, de quente a frio. | 0 - 5     |

---

### 3. Static Color (Cor Estática)
**Descrição:** Ilumina todos os LEDs com uma única cor sólida.
| Parâmetro   | Descrição                                         | Intervalo |
|-------------|---------------------------------------------------|-----------|
| `Hue`       | Seleciona a cor em todo o círculo cromático.      | 0 - 359   |
| `Saturation`| Controla a pureza da cor (de pálido a vibrante).  | 0 - 255   |

---

### 4. Christmas (Natal)
**Descrição:** Efeito festivo com fundo vermelho e verde e luzes brancas/douradas que piscam por cima.
| Parâmetro      | Descrição                                | Intervalo |
|----------------|------------------------------------------|-----------|
| `Twinkle Speed`| Controla a velocidade das luzes que piscam.| 1 - 50    |
| `Twinkles`     | Define a quantidade de luzes piscando.   | 0 - 20    |

---

### 5. Candle Math (Vela Matemática)
**Descrição:** Uma simulação de vela mais avançada e precisa, que usa modelos matemáticos para um efeito mais natural.
| Parâmetro   | Descrição                                                              | Intervalo    |
|-------------|------------------------------------------------------------------------|--------------|
| `Speed`     | Controla a velocidade do piscar da chama.                              | 1 - 50       |
| `Hue`       | Define a cor base da chama.                                            | 0 - 359      |
| `Saturation`| Controla a intensidade da cor da chama.                                | 0 - 255      |
| `Segments`  | Divide a fita em múltiplas chamas independentes.                       | 1 - (Nº LEDs)|
| `Intensity` | Controla a intensidade ou "altura" da tremulação da chama.             | 0 - 50       |
| `Dip Prob`  | Probabilidade de a chama "mergulhar" (perder brilho subitamente).      | 0 - 100      |

---

### 6. Christmas Twinkle (Pisca-Pisca de Natal)
**Descrição:** Efeito clássico de pisca-pisca de Natal, com luzes vermelhas, verdes e brancas aleatórias.
| Parâmetro | Descrição                                        | Intervalo |
|-----------|--------------------------------------------------|-----------|
| `Speed`   | Controla a velocidade do pisca-pisca.            | 1 - 50    |
| `Density` | Controla quantas luzes ficam acesas ao mesmo tempo.| 1 - 20    |

---

### 7. Random Twinkle (Pisca-Pisca Aleatório)
**Descrição:** LEDs individuais acendem e apagam aleatoriamente com cores e durações variadas.
| Parâmetro      | Descrição                                                              | Intervalo |
|----------------|------------------------------------------------------------------------|-----------|
| `Probability`  | A chance de um novo LED começar a piscar.                              | 1 - 100   |
| `Speed`        | A velocidade com que os LEDs acendem e apagam.                         | 1 - 50    |
| `Max Twinkles` | O número máximo de LEDs que podem piscar simultaneamente.              | 1 - 50    |
| `Palette`      | Seleciona entre diferentes paletas de cores para as luzes que piscam. | 0 - 3     |

---

### 8. Breathing (Respiração)
**Descrição:** Cria um efeito de pulsação suave, onde a luz aumenta e diminui de brilho lentamente.
| Parâmetro   | Descrição                                 | Intervalo |
|-------------|-------------------------------------------|-----------|
| `Speed`     | Controla a velocidade da pulsação.        | 1 - 100   |
| `Hue`       | Define a cor da luz que pulsa.            | 0 - 359   |
| `Saturation`| Controla a intensidade da cor.            | 0 - 255   |
