#include "game.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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

void findRandom(char* word, Tree *wb);

Game* initGame(char* WBpath){
    
    Game* game = malloc(sizeof(Game));
    game->WB = Tloader(WBpath);
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
        for (char j = 0; j < 26; j++){
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

int validWord(char* input, Game* game){
    Tree* tree = game->WB;
    for (size_t i = 0; i < 6; i++){
        size_t index = input[i] - 'a';
        if (tree->child[index] != NULL)
            tree = tree->child[index];
        else
            return 0;
    }
    return 1;
}

char* GetInput (Game* game){
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
                if (validWord(word, game))
                    break;
                else{
                    printf("\nWord not found try again! or add it to the word bank\n");
                    for (char i = 0; i < len; i++)
                        printf("%c", word[i]);
                }
            }
            else{
                printf("\nword input isn't correct\n");
                for (char i = 0; i < len; i++)
                    printf("%c", word[i]);
            }
        }
        else if (input == '\b' && len > 0)
            word[len--] = 0;
        else {
            printf("\ncontinue typing your word\n");
            for (char i = 0; i < len; i++)
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
    if (strcmp(input, game->Hword) == 0)
        game->found = 1;
    return color;
}

void yellow(){
    printf("\033[1;33m");
}

void green(){
    printf("\033[1;32m");
}

void reset(){
    printf("\033[0m");
}

void printLine(char* color, Game* game, size_t index){
    for (size_t i = 0; i < 6; i++){
        printf("║ ");
        if (color[i] == 'g') green();
        else if (color[i] == 'y') yellow();
        printf("%c ",toupper(game->guessedWords[index][i]));
        reset();
    }
    printf("║\n");
}

void prettyPrint(char* color, Game* game){
    system("clear");
    printf("\n╔═══════════════════════╗\n");
    printf("║You have %d guesses left║\n", maxGuesses - game->nb_Guesses);
    printf("╠═══╦═══╦═══╦═══╦═══╦═══╣\n");
    int i = 0;
    for (; i < game->nb_Guesses; i++){
        printLine(color, game, i);
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

char playAgain(){
    char input = 0;
    while (input != 'Y' && input != 'n'){
        printf("Do you want to play again? (Y or n) ");
        scanf("%c", &input);
    }
    return (input == 'Y') ? 1 : 0;
}

void GameLoop(char* WBpath){
    Game* game = initGame(WBpath);
    char playing = 1;
    while (playing){
        while (game->nb_Guesses < maxGuesses){
            if (game->found == 1)
                break;
            char* input = GetInput(game);
            char* color = checkWord(input, game);
            free(input);
            game->nb_Guesses++;
            prettyPrint(color, game);
            free(color);
        }
        if (game->found)
            printf("wow you won!\n");
        else {
            printf("Tough Luck fella\nThe word was %s\n",
                   game->Hword);
        }
        playing = playAgain();
    }
    printf("Thanks for playing!\n");
    freeGame(game);
}






