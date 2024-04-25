#include "game.h"
#include <string.h>


/* find a random word to choose from the tree struct or the word bank
* check if an inputted word is added
* from the secret word find the letters that are : (take care if there are double letters or more)
*                                                  -in the right place (green)
                                                  -in the word but at the wrong place (yellow)
*                                                  -not in the word
* Game loop :
*  -> Initialize the Game
*      .> load the word bank into a GTree
*      .> set the nb guesses to 0 
*      .> choose the word to find
*      .> set found word to 0
*      .> set up an array of guessed words
*  :> while playing :
*     -> while the word has not been found or the nb guesses is different than max Guesses
*         .> Ask for user input to build a six letter word
*             |> check if the word is long enough and if so check if it belongs to the WB
*             |> if not => Ask for user input again
*         .> update game state => array of guessed words + validate the word + nb guesses
*         .> (Terminal Version) Pretty print the results with color changes in the terminal
*             nice boxes + number of guesses
*     -> if the word was not found 
*          .> pretty print the hidden word
*     -> ask if the user wants to play again:
*          .> reset the array of guessed words
*          .> find a new word to choose from the WB
*          .> reset the nb guessed words
*  => free the game :
*       -> free the GTree
*       -> free Game struct
*/
void yellow(){
    printf("\033[1;33m");
}

void red(){
    printf("\033[1;31m");
}

void green(){
    printf("\033[1;32m");
}

void black(){
    printf("\033[1;30m");
}

void reset(){
    printf("\033[0m");
}

void printLine(Game* game, int index){
    for (size_t i = 0; i < 6; i++){
        printf("║ ");
        if (game->colorWords[index][i] == 'g') green();
        else if (game->colorWords[index][i] == 'y') yellow();
        else if (game->colorWords[index][i] == 'r') red();
        if (game->guessedWords[index][i] == 0)
            printf("  ");
        else
            printf("%c ",toupper(game->guessedWords[index][i]));
        reset();
    }
    if (index == game->nb_Guesses)
        printf("║«\n");
    else
        printf("║\n");
}

void prettyPrint(Game* game){
    system("clear");
    //printf("%s\n", game->Hword);
    //printf("%s\n", game->guessedWords[game->nb_Guesses]);
    printf("╔═══════════════════════╗\n");
    printf("║You have %d guesses left║\n", maxGuesses - game->nb_Guesses);
    printf("╠═══╦═══╦═══╦═══╦═══╦═══╣\n");
    int i = 0;
    for (; i <= game->nb_Guesses; i++){
        if (i == maxGuesses)
            continue;
        printLine(game, i);
        if (i == maxGuesses - 1)
            printf("╚═══╩═══╩═══╩═══╩═══╩═══╝\n");
        else
            printf("╠═══╬═══╬═══╬═══╬═══╬═══╣\n");
    }
    for (; i < maxGuesses; i++){
        printf("║   ║   ║   ║   ║   ║   ║\n");
        if (i == maxGuesses - 1)
            printf("╚═══╩═══╩═══╩═══╩═══╩═══╝\n");
        else
            printf("╠═══╬═══╬═══╬═══╬═══╬═══╣\n");
    }
}

void setCharColor(Game* game, char c){
    char color = game->colorLetters[c - 'a'];
    switch (color) {
        case 'y':
            yellow();
            break;
        case 'g':
            green();
            break;
        case 'w':
            black();
            break;
        default:
            reset();
            break;
    }
    printf("%c ", c);
}

void printKeys(Game* game){
    // QWERTY Layout
    // First ROW
    printf("   ");
    setCharColor(game, 'q');
    setCharColor(game, 'w');
    setCharColor(game, 'e');
    setCharColor(game, 'r');
    setCharColor(game, 't');
    setCharColor(game, 'y');
    setCharColor(game, 'u');
    setCharColor(game, 'i');
    setCharColor(game, 'o');
    setCharColor(game, 'p');
    printf("\n");

    // Second Row
    printf("    ");
    setCharColor(game, 'a');
    setCharColor(game, 's');
    setCharColor(game, 'd');
    setCharColor(game, 'f');
    setCharColor(game, 'g');
    setCharColor(game, 'h');
    setCharColor(game, 'j');
    setCharColor(game, 'k');
    setCharColor(game, 'l');
    printf("\n");

    // Thrid Row
    printf("      ");
    setCharColor(game, 'z');
    setCharColor(game, 'x');
    setCharColor(game, 'c');
    setCharColor(game, 'v');
    setCharColor(game, 'b');
    setCharColor(game, 'n');
    setCharColor(game, 'm');
    printf("\n");

    reset();
}

void blackenKeys(Game* game){
    for (size_t i = 0; i < 6; i++){
        if (game->colorWords[game->nb_Guesses][i] == 'b'){
            int index = game->guessedWords[game->nb_Guesses][i] - 'a';
            game->colorLetters[index] = 'w';
        }
    }
}

void findRandom(char* word, Tree *wb);

Game* initGame(char* WBpath){
    
    Game* game = malloc(sizeof(Game));
    game->WB = Tloader(WBpath);
    game->Solutions = Tloader("obj/solutions.txt");
    game->found = 0;
    game->nb_Guesses = 0;
    
    for (size_t i = 0; i < maxGuesses; i++){
        for (size_t j = 0; j < 7; j++){
            game->guessedWords[i][j] = 0;
        }
        for (size_t j = 0; j < 6; j++){
            game->colorWords[i][j] = 'b';
        }
    }
    for (size_t i = 0; i < 26; i++){
        game->colorLetters[i] = 'b';
    }
    findRandom(game->Hword, game->Solutions);
    return game;
}

void freeGame(Game* game){
    freeTree(game->WB);
    freeTree(game->Solutions);
    free(game);

}

void findRandom(char *word, Tree *wb){
    unsigned int randi;
    word[6] = 0;
    for (size_t i = 0; i < 6; i++){
        unsigned char m[26] = {0, };
        unsigned char nbW = 0;
        for (unsigned char j = 0; j < 26; j++){
            if (wb->child[j] != NULL){
                m[nbW] = j;
                nbW++;
            }
        }
        randi = randomNumber() % nbW;
        word[i] = m[randi] + 'a';
        wb = wb->child[m[randi]];
    }

}

int validWord(Game* game){
    Tree* tree = game->WB;
    for (size_t i = 0; i < 6; i++){
        size_t index = game->guessedWords[game->nb_Guesses][i] - 'a';
        if (tree->child[index] != NULL)
            tree = tree->child[index];
        else
            return 0;
    }
    return 1;
}

int getch(){
    struct termios oldtc, newtc;
    int ch;
    tcgetattr(STDIN_FILENO, &oldtc);
    newtc = oldtc;
    newtc.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newtc);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldtc);
    return ch;
}

char isDuplicate(Game* game){
    for (int i = 0; i < game->nb_Guesses; i++){
        if (strcmp(game->guessedWords[i], game->guessedWords[game->nb_Guesses]) == 0)
            return 1;
    }
    return 0;
}


void GetInput (Game* game){
    int input = 0;
    unsigned char len = 0;
    while (1){
        input = getch();
        if (input >= 'a' && input <= 'z' && len < 6){
            game->guessedWords[game->nb_Guesses][len] = input;
            len++;
        }
        else if (input >= 'A' && input <= 'Z' && len < 6){
            game->guessedWords[game->nb_Guesses][len] = tolower(input);
            len++;
        }
        else if (input == '\n'){
            if (len == 6){
                if (validWord(game) && !isDuplicate(game))
                    break;
                else{
                    for (size_t i = 0; i < 6; i++){
                        game->colorWords[game->nb_Guesses][i] = 'r';
                    }
                    prettyPrint(game);
                    printKeys(game);
                    sleep(1);
                    for (size_t i = 0; i < 6; i++){
                        game->colorWords[game->nb_Guesses][i] = 'b';
                    }
                    prettyPrint(game);
                    printKeys(game);
                    continue;
                }
            }
            else{
                continue;
            }
        }
        else if ((input == ']' || input == '*')  && len > 0){
            game->guessedWords[game->nb_Guesses][--len] = 0;
        }
        prettyPrint(game);
        printKeys(game);
    }
}

void colorWord(Game* game){
    // Create a histogram of letters for both hidden and input word
    char mH[26] = {0,};
    char mG[26] = {0,};
    for (size_t i = 0; i < 6; i++){
        mH[game->Hword[i] - 'a']++;
        mG[game->guessedWords[game->nb_Guesses][i] - 'a']++;
    }
    // loop for correct position words
    for (size_t i = 0; i < 6; i++){
        if (game->guessedWords[game->nb_Guesses][i] == game->Hword[i]){
            mH[game->Hword[i] - 'a']--;
            mG[game->Hword[i] - 'a']--;
            
            game->colorLetters[game->Hword[i] - 'a'] = 'g';
            game->colorWords[game->nb_Guesses][i] = 'g';
        }    
    }

    //loop for letters at the wrong place
    for (size_t i = 0; i < 6; i++){
        if (game->colorWords[game->nb_Guesses][i] == 'b'){
            for (size_t j = 0; j < 6; j++){
                if (game->guessedWords[game->nb_Guesses][i] == game->Hword[j] &&
                mH[game->Hword[j] - 'a'] > 0){

                    mG[game->guessedWords[game->nb_Guesses][i] - 'a']--;
                    mH[game->Hword[j] - 'a']--;
                    game->colorWords[game->nb_Guesses][i] = 'y';
                    if (game->colorLetters[game->Hword[j] - 'a'] != 'g')
                        game->colorLetters[game->Hword[j] - 'a'] = 'y';
                    break;
                }
            }
        }
    }
    if (strcmp(game->guessedWords[game->nb_Guesses], game->Hword) == 0)
        game->found = 1;
}



char endGame(Game* game){
    system("clear");
    prettyPrint(game);
    if (game->found == 0){
        printf("╔═══════════════════════╗\n");
        printf("║     The word was      ║\n");
        printf("║        %s         ║\n", game->Hword);
        printf("╚═══════════════════════╝\n");
    }
    else {
        printf("╔═══════════════════════╗\n");
        printf("║ Congrats you got it ! ║\n");
        printf("╚═══════════════════════╝\n");
    }
    int input = 0;
    printf("╔═══════════════════════╗\n");
    printf("║Want to play some more?║\n");
    printf("║     (Y)es | (n)o      ║\n");
    printf("╚═══════════════════════╝\n");
    while(1){
        input = getch();
        if (input == 'y' || input == 'Y')
            return 1;
        else if (input == 'n' || input == 'N')
            return 0;
    }
}

void GameLoop(char* WBpath){
    Game* game = initGame(WBpath);
    char playing = 1;
    while (playing){
        prettyPrint(game);
        printKeys(game);
        while (game->nb_Guesses < maxGuesses){
            if (game->found == 1)
                break;
            GetInput(game);
            blackenKeys(game);
            colorWord(game);
            game->nb_Guesses++;
            if (game->nb_Guesses != maxGuesses){
                prettyPrint(game);
                printKeys(game);
            }
        }
        prettyPrint(game);
        printKeys(game);
        playing = endGame(game);
        if (playing == 1){
            freeGame(game);
            game = initGame(WBpath);
            /*for (size_t i = 0; i < maxGuesses; i++){
                for (size_t j = 0; j < 7; j++){
                    game->guessedWords[i][j] = 0;
                }
                for (size_t j = 0; j < 6; j++){
                    game->colorWords[i][j] = 'b';
                }
            }
            findRandom(game->Hword, game->WB);
            game->nb_Guesses = 0;*/
        }
    }
    printf("Thanks for playing!\n");
    freeGame(game);
}
