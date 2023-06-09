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

#define maxGuesses 5
