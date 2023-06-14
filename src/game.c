#include "game.h"

/* find a random word to choose from the tree struct or the word bank
* check if an inputted word is added
* from the secret word find the letters that are : (take care if there are double letters or more)
*                                                  -in the right place (green)
*                                                  -in the word but at the wrong place (yellow)
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

void findRandom(char word[7], Tree *wb);

Game* initGame(char* WBpath){
    
    Game* game = malloc(sizeof(Game));
    game->WB = Tloader(WBpath);
    game->found = 0;
    game->nb_Guesses = 0;
    
    for (size_t i = 0; i < maxGuesses; i++){
        for (size_t j = 0; j < 7; j++){
            game->guessedWords[i][j] = 0;
        }
    }
    findRandom(game->Hword, game->WB);
    return game;
}

void freeGame(Game* game){
    freeTree(game->WB);
    free(game);
}

void findRandom(char *word, Tree *wb){
    int randi;
    word[6] = 0;
    for (size_t i = 0; i < 6; i++){
        char m[26] = {0, };
        char nbW = 0;
        for (size_t j = 0; j < 26; j++){
            if (wb->child[j] != NULL){
                m[nbW] = j;
                nbW++;
            }
        }
        randi = rand() % nbW;
        word[i] = m[randi] + 'a';
        wb = wb->child[m[randi]];
    }

}

static char* GetInput (){
    char* word = calloc(7, sizeof(char));
    char input = 0;
    char len = 0;
    while (1){
        input = getchar();
        if (input >= 'a' && input <= 'z'){
            word[len++] = input;
        }
        else if (input >= 'A' && input <= 'Z'){
            word[len++] = tolower(input);
        }
        else if (input == '\n'){
            if (len == 6){
                word[len] = 0;
                break;
            }
            else{
                printf("\nword input isn't correct\n");
                for (size_t i = 0; i < len; i++)
                    printf("%c", word[i]);
            }
        }
        else if (input == '\b' && len > 0)
            word[len--] = 0;
        else {
            printf("\ncontinue typing your word\n");
            for (size_t i = 0; i < len; i++)
                printf("%c", word[i]);
        }
    }
    return word;
}

char* checkWord(char* input, Game* game){
    char* color = calloc(6, sizeof(char));
    char m[6] = {-1,}; //-1 not found / 0 found but wrong place / 1 right place
    char f = 0;
    for (size_t i = 0; i < 6; i++){
        game->guessedWords[game->nb_Guesses][i] = input[i];
        if (input[i] == game->Hword[i]){
            m[i] = 1;
            color[i] = 'g';
            f++;
        }
        else {
            int change = 0;
            for (size_t j = 0; j < 6; j++){
                if (input[i] == game->Hword[j]){
                    if (m[i] == -1){
                        m[i] = 0;
                        color[i] = 'y';
                        change = 1;
                        break;
                    }
                }
            }
            if (!change){
                color[i] = 'b';
            }
        }
    }
    if (f == 5)
        game->found = 1;
    return color;
}

void prettyPrint(char* input, char* color){
    // TODO
}

void GameLoop(char* WBpath){
    Game* game = initGame(WBpath);
    char playing = 1;
    while (playing){
        while (game->found && game->nb_Guesses < maxGuesses){
            char* input = GetInput();
            char* color = checkWord(input, game);
            // TODO call prettyPrint(input, color);
            free(color);
            free(input);
            game->nb_Guesses++;
        }
        if (game->found)
            printf("wow you won!");
        else {
            // TODO
        }
        // TODO: Ask to play again
    }
    freeGame(game);
}






