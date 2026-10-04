#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h> 

#define SAMPLE_RATE 44100
#define TAU 6.283185307179586f
#define SEMITONE_FACTOR 1.05946309436f
#define BUFFER_SIZE 256

// comando d reproduccion
char audioCommand[1024];
// array de frecuencias
float chromaticScale[256] = {0.0f};
// terminal de backup para q funque cuando volvamos del raw mode
struct termios backupTerminal;
// factor de octava, por defecto 1 para central
float octaveFactor = 1.0f;
// factor de semitono
float semitoneFactor = 1.0f;
int semitoneShift = 0;
// factor de distorpawer
float gainFactor = 1.0f;
// factor de volumen
float volFactor = 0.7f;
// control d onda (1 seno, 2 cuadrada, 3 triangulo, 4 sierra)
int waveMode = 1;
// profundidad del vibrato
float vibratoDepth = 0.0f;
// fase del lfo
float lfoPhase = 0.0f;
// env global pa evitar el chasquido
float currentEnvelope = 0.0f;

// raw mode
void initRawMode(){
    struct termios rawMode;
    // respaldamos configuracion de terminal
    tcgetattr(STDIN_FILENO, &backupTerminal);

    // clonamos para modificar la copia
    rawMode = backupTerminal;
    
    // apagamos el enter (ICANON) y el dibujado (ECHO)
    rawMode.c_lflag &= ~(ICANON | ECHO);

    // lectura no bloqueante
    rawMode.c_cc[VMIN] = 0;
    rawMode.c_cc[VTIME] = 0;

    // inyectamos cambios
    tcsetattr(STDIN_FILENO, TCSANOW, &rawMode);

    // activamos trackeo de mouse
    printf("\033[?1000h\033[?1006h");
    fflush(stdout);

    system("xset r rate 150 50 2>/dev/null");
}

// devolver terminal a la normalidad
void returnCookedMode(){
    printf("\033[2J\033[H"); 
    printf("\033[?1000l\033[?1006l");
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSANOW, &backupTerminal);
    system("xset r rate 500 33 2>/dev/null"); 
}

void drawInterface(float vol, float oct, float gain, int wave, float vib, int shift) {
    printf("\033[2J\033[H"); // limpia terminal y resetea cursor
    printf("==============================================\n");
    printf("    CliSynth: Interactive terminal synth      \n");
    printf("==============================================\n\n");
    
    // muestra los parámetros actuales
    char* waveName = (wave==1) ? "SINE" : (wave==2) ? "SQUARE" : (wave==3) ? "TRIANGLE" : "SAWTOOTH";
    printf("  [ Vol: %.1f ] [ Octave: x%.2f ] [ Gain: %.1f ] [ Semi: %+d ]\n", vol, oct, gain, shift);
    printf("  [ Waveform (1-4): %s ] [Vibrato Mod Wheel: %.1f ]\n\n", waveName, vib * 10.0f);
    printf("Top letter: Note | Under letter: Execution key\n");
    printf("Vol Up/Down: B/V | Octave Up/Down: +/- | Overdrive On/Off: Z/X\n");
    printf("Semitone Up/Down: 0/9 | Vibrato Up/Down: Mouse Wheel\n\n");
    printf("┌──┬──┬┬──┬─┬──┬──┬┬──┬┬──┬─┬───┐\n");
    printf("│  │C#││D#│ │  │F#││G#││A#│ │   │\n");
    printf("│  │W ││E │ │  │T ││Y ││U │ │   │\n");
    printf("│  └┬─┘└┬─┘ │  └┬─┘└┬─┘└┬─┘ │   │\n");
    printf("│C  │D  │E  │F  │G  │A  │B  │C  │\n");
    printf("│A  │S  │D  │F  │G  │H  │J  │K  │\n");
    printf("└───┴───┴───┴───┴───┴───┴───┴───┘\n\n");
    printf("Press 'q' to quit\n");
    fflush(stdout);
}

int main(){
    // frecuencia base
    float baseHz = 0.0f;
    // la fase (posicion de la onda)
    float phase = 0.0f;

    signal(SIGPIPE, SIG_IGN);

    sprintf(audioCommand, "aplay -f U8 -r %d -c 1 --buffer-size=512 --disable-resample - 2>/dev/null", SAMPLE_RATE);

    // pipe de aplay
    FILE * pipeOut = popen(audioCommand, "w");
    if (pipeOut == NULL){
        printf("aplay not found. exit...");
        return 1;
    }

    // reducimos el tamaño del pipe a 1024 para menos delay
    int fd = fileno(pipeOut);
    fcntl(fd, F_SETPIPE_SZ, 1024);

    // iniciamos raw mode 
    initRawMode();

    // mapeo de teclas (awsedftgyhjk)
    chromaticScale['a'] = 261.63f; // do
    chromaticScale['w'] = 277.18f; // do#
    chromaticScale['s'] = 293.66f; // re
    chromaticScale['e'] = 311.13f; // re#
    chromaticScale['d'] = 329.63f; // mi
    chromaticScale['f'] = 349.23f; // fa
    chromaticScale['t'] = 369.99f; // fa#
    chromaticScale['g'] = 391.85f; // sol
    chromaticScale['y'] = 415.30f; // sol#
    chromaticScale['h'] = 440.00f; // la 
    chromaticScale['u'] = 466.16f; // la#
    chromaticScale['j'] = 493.88f; // si
    chromaticScale['k'] = 523.26f; // do

    drawInterface(volFactor, octaveFactor, gainFactor, waveMode, vibratoDepth, semitoneShift);

    // indice de ciclos para apagar el sonido
    int activeDecay = 0;

    unsigned char buffer[BUFFER_SIZE];

    // bucle d reproduccion
    while(1){
        char pressedKey = 0;
        // read intenta buscar un byte del teclado 
        int isRead = read(STDIN_FILENO, &pressedKey, 1);

        // solo procesamos el teclado si realmente el sensor detecto un byte valido (> 0)
        if (isRead > 0) {
            int paramChanged = 0; // flag pa ver si los parametros cambiaron

            // el cosito del cosito del mouse o como se diga
            if (pressedKey == '\033') {
                char seq[64] = {0}; // buffer de escape seguro
                int n = read(STDIN_FILENO, seq, sizeof(seq) - 1);
                
                if (n > 2 && seq[0] == '[' && seq[1] == '<') {
                    // parseo del nro de boton sgr
                    int button = atoi(&seq[2]);
                    
                    // codigo 64 rueda arriba, vibrato arriba
                    if (button == 64 && vibratoDepth < 1.0f) {
                        vibratoDepth += 0.1f;
                        if (vibratoDepth > 1.0f) vibratoDepth = 1.0f;
                        paramChanged = 1;
                    }
                    // codigo 65 es rueda abajo, bajamos vibrato
                    if (button == 65 && vibratoDepth > 0.0f) {
                        vibratoDepth -= 0.1f;
                        if (vibratoDepth < 0.0f) vibratoDepth = 0.0f;
                        paramChanged = 1;
                    }
                }
                
                // mandamos a la mierda a la basura
                char trash;
                while (read(STDIN_FILENO, &trash, 1) > 0); 
            }

            // si es q salimos
            if (pressedKey == 'q') {
                printf("\nThanks for using! Exit...\n");
                break; 
            }
            
            // control de octava
            // si tocan -, bajamos uno
            if(pressedKey == '-' && octaveFactor >= 0.125f){
                octaveFactor *= 0.5f;
                paramChanged = 1;
            }
            // si es +, subimos uno
            if (pressedKey =='+' && octaveFactor <= 8.0f){
                octaveFactor *= 2.0f;
                paramChanged = 1;
            }

            // control de semitono
            // si es 9 bajamos
            if (pressedKey == '9' && semitoneShift > -12){
                semitoneFactor /= SEMITONE_FACTOR;
                semitoneShift --;
                paramChanged = 1;
            }
            // si es 0 subimos
            if (pressedKey == '0' && semitoneShift < 12) {
                semitoneFactor *= SEMITONE_FACTOR;
                semitoneShift++;
                paramChanged = 1;
            }

            // control de overdrive
            // si apretan z lo activamos
            if(pressedKey == 'z' && gainFactor < 10.0f){
                gainFactor += 1.0f;
                paramChanged = 1;
            }
            // si apretan x lo desactivamos
            if(pressedKey == 'x' && gainFactor > 1.0f){
                gainFactor -= 1.0f;
                paramChanged = 1;
            }

            // control de volumen
            // si apretan v bajamos
            if(pressedKey == 'v' && volFactor > 0.0f){
                volFactor -= 0.1f;
                if (volFactor < 0.0f) volFactor = 0.0f;
                paramChanged = 1;
            }
            // si apretan b lo subimos
            if(pressedKey == 'b' && volFactor < 1.0f){
                volFactor += 0.1f;
                if (volFactor > 1.0f) volFactor = 1.0f;
                paramChanged = 1;
            }

            // si apreetan p silenciamos
            if (pressedKey == 'p'){
                baseHz = 0.0f;
            }
            // control d onda
            if (pressedKey == '1') { waveMode = 1; paramChanged = 1; } // seno
            if (pressedKey == '2') { waveMode = 2; paramChanged = 1; } // cuadrado
            if (pressedKey == '3') { waveMode = 3; paramChanged = 1; } // triangulo
            if (pressedKey == '4') { waveMode = 4; paramChanged = 1; } // sierra

            // extraemos primero a una variable temporal
            float keyHz = chromaticScale[(unsigned char)pressedKey];

            // solo si la tecla presionada corresponde a una nota afinada (> 0.0f), actualizamos el oscilador
            if (keyHz > 0.0f) {
                // definimos a baseHz como el valor del indice d chromaticScale correspondiente al codigo ascii de la tecla
                baseHz = keyHz * octaveFactor * semitoneFactor;
                activeDecay = 35; // release corto delegado a la env por software
            }

            // refrescamos los parametros de la pantalla si hubo algun cambio
            if (paramChanged) {
                drawInterface(volFactor, octaveFactor, gainFactor, waveMode, vibratoDepth, semitoneShift);
            }
        } 
        
        // el else va atado a si el sensor no detecto teclas en esta vuelta
        if (isRead <= 0) {
            // si se deja de tocar la tecla se baja a active decay d a poco
            if (activeDecay > 0) {
                activeDecay--;
            }
        }

        // motor de vibrato
        for(int i = 0; i < BUFFER_SIZE; i++){
            if (baseHz == 0.0f){
                buffer[i] = 127; // si hay silencio, dejamos la membrana del parlante en reposo
                lfoPhase = 0.0f; // reseteamos el lfo en el silencio
                currentEnvelope = 0.0f; // rst del amp env
            } else {
                // rampa de env adsr por muestra pa matar clicks
                if (activeDecay > 0) {
                    currentEnvelope += 0.01f; // attack
                    if (currentEnvelope > 1.0f) currentEnvelope = 1.0f;
                } else {
                    currentEnvelope -= 0.001f; // release suave
                    if (currentEnvelope <= 0.0f) {
                        currentEnvelope = 0.0f;
                        baseHz = 0.0f; // kill de osc al llegar a 0 de amp
                    }
                }

                // dejamos el incremento
                float lfoIncrement = (TAU * 6.0f) / SAMPLE_RATE;
                float lfoWave = sinf(lfoPhase);
                lfoPhase += lfoIncrement;
                if (lfoPhase >= TAU) lfoPhase -= TAU;

                // frecuencia modulada como el tema de seru
                float modulatedHz = baseHz + (baseHz * 0.05f * lfoWave * vibratoDepth);
                float phaseIncrement = (TAU * modulatedHz) / SAMPLE_RATE;

                float wave = 0.0f; 
                
                if (waveMode == 1) {
                    // 1 senoidal 
                    wave = sinf(phase);
                } 
                else if (waveMode == 2) {
                    // 2 cuadrada 
                    wave = (phase < TAU / 2.0f) ? 1.0f : -1.0f;
                } 
                else if (waveMode == 3) {
                    // 3 triangular 
                    wave = (phase < TAU / 2.0f) ? (-1.0f + (4.0f * phase) / TAU) : (3.0f - (4.0f * phase) / TAU);
                } 
                else if (waveMode == 4) {
                    // 4 de sierra 
                    wave = 1.0f - (2.0f * phase / TAU);
                }

                // un poco de gain
                wave = wave * gainFactor;
                if (wave > 1.0f)  wave = 1.0f;
                if (wave < -1.0f) wave = -1.0f;

                // cmultiplicamos por el volumen antes del mapeo
                wave = wave * volFactor * currentEnvelope;
                
                buffer[i] = (unsigned char)((wave + 1.0f) * 127.5f); // mapeo a byte
                phase += phaseIncrement; 
                if (phase >= TAU){
                    phase -= TAU;
                }
            }
        }
        // mandamos el buffer por la pipe
        fwrite(buffer, 1, BUFFER_SIZE, pipeOut);
        fflush(pipeOut);
    }

    // volvemos a cooked mode
    returnCookedMode();
    // cerramos el pipe
    pclose(pipeOut);
    return 0;
}