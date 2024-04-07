
KFinder - File Search App
Overview
KFinder is a lightweight file search application designed to simplify the process of finding files on your computer. Whether you're looking for documents, images, music, or any other file type, KFinder provides a quick and efficient way to locate them.

Installation
Clone Repository or Download Zip
Clone this repository into a directory on your computer or download the zip file.
Set Environment Variables
Copy the path to the bin folder of KFinder.
Add the copied path to your environment variables. This step is essential for running KFinder from any location in your terminal.
Running KFinder
Open your terminal and navigate to the folder you want to index.
Run the command kfinder init to initialize indexing for the directory.
Run kfinder index to start indexing the files in the directory.
Optionally, if you want to index the same directory as where KFinder is running, simply type ./.
After this initial setup, you won't need to repeat the indexing process for the same folder again.
Usage
Searching for Files
To search for files, simply type kfinder search followed by your search query in the terminal.
KFinder will display a list of matching files along with their index numbers.
Opening Files
Once you have your search results, input the index number of the file you want to open.
KFinder will open the selected file for you.
