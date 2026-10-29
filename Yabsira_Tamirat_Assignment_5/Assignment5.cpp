/*
Program: EECS 348 Assignment 5 (revision of another student's Assignment 4)
Description: Sudoku solver that uses recursive depth-first search with
             backtracking to find every solution to each puzzle file
Inputs: puzzle1.txt through puzzle5.txt by default, or puzzle files named
        on the command line; '_' marks a blank cell
Outputs: Console report for each puzzle (file name, puzzle, solutions or
         "No solution found"); a solutionN.txt file for each puzzle holding
         every solution in the puzzle-file format followed by
         "Solutions found: n", or the single line "No solution found."
Collaborators: None
Other Sources: None
Author: Alan Thomas
Creation Date: October 15, 2026
Revised by: Yabsira Tamirat
Revision Date: October 27, 2026
Revisions: Added Board::printPlain() to print a grid in the same format as
           the puzzle files; PuzzleRunner now writes each puzzle's results
           to solutionN.txt, where N is the puzzle number taken from the
           file name
*/

#include <array>        // std::array: fixed-size rows of the grid
#include <cctype>       // std::isspace, std::isprint: character classification
#include <filesystem>   // std::filesystem: check that a file exists and is a regular file
#include <fstream>      // std::ifstream: reading puzzle files
#include <iostream>     // std::cout, std::ostream: printing results
#include <stdexcept>    // std::runtime_error, std::exception: error handling
#include <string>       // std::string: file names and messages
#include <system_error> // std::error_code: non-throwing filesystem checks
#include <vector>       // std::vector: candidate lists and solution lists

class PuzzleException : public std::runtime_error
{                                                        // Custom exception derived from runtime_error
public:                                                  // Public members follow
    explicit PuzzleException(const std::string &message) // Constructor takes the error message
        : std::runtime_error(message)
    {
    } // Pass the message to the base class
}; // End of PuzzleException

class Board
{                               // Start of the Board class
public:                         // Public members follow
    static const int SIZE = 9;  // Number of rows and columns in the grid
    static const int BLOCK = 3; // Width and height of one sub-grid (block)

    Board()
    { // Default constructor
        for (auto &row : grid)
            row.fill(0); // Start with every cell empty (0 means empty)
    } // End of constructor

    // Reads a puzzle from a file. Digits 1-9 are givens; '_' is blank.  // Describes load()
    // Throws PuzzleException with a specific message if anything is wrong.  // Describes the error behaviour
    void load(const std::string &filename)
    {                                   // Load a puzzle from the named file
        namespace fs = std::filesystem; // Short alias for the filesystem namespace
        std::error_code ec;             // Receives filesystem errors without throwing

        if (filename.empty())                                    // Edge case: an empty file name was supplied
            throw PuzzleException("no file name was given");     // Report the problem
        if (!fs::exists(filename, ec) || ec)                     // Edge case: file is missing (or the check itself failed)
            throw PuzzleException("file does not exist");        // Report the problem
        if (!fs::is_regular_file(filename, ec) || ec)            // Edge case: path is a directory or special file
            throw PuzzleException("path is not a regular file"); // Report the problem

        std::ifstream in(filename, std::ios::binary);                              // Open in binary mode so we see raw bytes (\r, BOM)
        if (!in)                                                                   // Edge case: file exists but cannot be opened (e.g. permissions)
            throw PuzzleException("file could not be opened (check permissions)"); // Report the problem

        char bom[3] = {0, 0, 0};                                    // Buffer for a possible UTF-8 byte-order mark
        in.read(bom, 3);                                            // Try to read the first three bytes
        bool hasBom = in.gcount() == 3 &&                           // A BOM needs exactly three bytes...
                      static_cast<unsigned char>(bom[0]) == 0xEF && // ...first byte 0xEF
                      static_cast<unsigned char>(bom[1]) == 0xBB && // ...second byte 0xBB
                      static_cast<unsigned char>(bom[2]) == 0xBF;   // ...third byte 0xBF
        if (!hasBom)
        {                // If there was no BOM, those bytes are real content
            in.clear();  // Clear the EOF flag in case the file was shorter than 3 bytes
            in.seekg(0); // Go back to the start so nothing is lost
        } // End of BOM handling

        std::array<std::array<int, SIZE>, SIZE> temp; // Read into a temporary grid so errors leave this Board untouched
        for (auto &row : temp)
            row.fill(0); // Initialise the temporary grid to empty
        int count = 0;   // How many cells have been read so far
        int line = 1;    // Current line number, used in error messages
        char ch;         // The character currently being examined

        while (in.get(ch))
        { // Read the file one character at a time
            if (ch == '\n')
            {             // A newline moves to the next line
                ++line;   // Track the line number
                continue; // Nothing else to do for a newline
            } // End of newline handling
            if (std::isspace(static_cast<unsigned char>(ch))) // Spaces, tabs and '\r' (Windows) are ignored
                continue;                                     // Skip whitespace

            bool isDigit = (ch >= '1' && ch <= '9');              // Is this a given digit 1-9?
            bool isBlank = (ch == '_' || ch == '0' || ch == '.'); // Accept '_' (required) plus '0' and '.' as blanks

            if (!isDigit && !isBlank)
            {                                                                                             // Edge case: any other character is invalid
                std::string shown = std::isprint(static_cast<unsigned char>(ch))                          // Show printable characters directly
                                        ? std::string("'") + ch + "'"                                     // Printable: wrap in quotes
                                        : std::string("a non-printable character");                       // Otherwise describe it generically
                throw PuzzleException("invalid character " + shown + " on line " + std::to_string(line)); // Report the problem
            } // End of invalid-character check

            if (count >= SIZE * SIZE)                                                                                // Edge case: more than 81 cells in the file
                throw PuzzleException("more than 81 cells found (extra data on line " + std::to_string(line) + ")"); // Report the problem

            temp[count / SIZE][count % SIZE] = isDigit ? (ch - '0') : 0; // Store the digit, or 0 for a blank
            ++count;                                                     // One more cell has been read
        } // End of read loop

        if (in.bad())                                                                             // Edge case: a low-level I/O error happened mid-read
            throw PuzzleException("I/O error while reading the file");                            // Report the problem
        if (count == 0)                                                                           // Edge case: the file is empty or only whitespace
            throw PuzzleException("file is empty");                                               // Report the problem
        if (count < SIZE * SIZE)                                                                  // Edge case: not enough cells
            throw PuzzleException("only " + std::to_string(count) + " cells found; expected 81"); // Report the problem

        grid = temp; // Everything was valid, so commit the new grid
    } // End of load()

    int get(int r, int c) const { return grid[r][c]; }           // Read the digit at row r, column c (0 = empty)
    void set(int r, int c, int v) { grid[r][c] = v; }            // Write digit v at row r, column c
    bool isEmpty(int r, int c) const { return grid[r][c] == 0; } // True when the cell has no digit

    // Can digit v legally be placed at (r, c) given the current grid?  // Describes canPlace()
    bool canPlace(int r, int c, int v) const
    { // Check row, column and block for v
        for (int i = 0; i < SIZE; ++i)
        { // Walk along the row and column together
            if (grid[r][i] == v)
                return false; // v already appears in this row
            if (grid[i][c] == v)
                return false; // v already appears in this column
        } // End of row/column loop
        int br = (r / BLOCK) * BLOCK;       // Top row of the block containing (r, c)
        int bc = (c / BLOCK) * BLOCK;       // Left column of the block containing (r, c)
        for (int i = 0; i < BLOCK; ++i)     // Loop over the block's rows
            for (int j = 0; j < BLOCK; ++j) // Loop over the block's columns
                if (grid[br + i][bc + j] == v)
                    return false; // v already appears in this block
        return true;              // No conflicts, so v can be placed
    } // End of canPlace()

    // Assignment 5 revision (Yabsira Tamirat): count the legal digits for  // Who wrote this block
    // cell (r, c) with a plain counter instead of building a std::vector every time.  // Why it changed (no memory allocation)
    int countCandidates(int r, int c) const
    {                                   // How many digits could go in this cell
        int count = 0;                  // Running total of legal digits
        for (int v = 1; v <= SIZE; ++v) // Try each digit 1-9
            if (canPlace(r, c, v))
                ++count; // Count it if it does not conflict
        return count;    // 0 means a dead end
    } // End of countCandidates()

    // Finds the empty cell with the fewest candidates (MRV heuristic).  // Describes findBestEmptyCell()
    // Returns false if the board is full.  // Describes the return value
    bool findBestEmptyCell(int &outR, int &outC) const
    {                        // Output the chosen row and column
        int best = SIZE + 1; // Fewest candidates seen so far (start above the maximum of 9)
        bool found = false;  // Becomes true once any empty cell is seen
        for (int r = 0; r < SIZE; ++r)
        { // Scan every row
            for (int c = 0; c < SIZE; ++c)
            { // Scan every column
                if (!isEmpty(r, c))
                    continue;                  // Skip cells that already have a digit
                int n = countCandidates(r, c); // How many digits could go here (Assignment 5: no vector)
                if (n < best)
                {                 // Is this the most constrained cell so far?
                    best = n;     // Remember its candidate count
                    outR = r;     // Remember its row
                    outC = c;     // Remember its column
                    found = true; // At least one empty cell exists
                    if (best == 0)
                        return true; // Zero candidates is a dead end; stop looking
                } // End of better-cell check
            } // End of column loop
        } // End of row loop
        return found; // False means the board has no empty cells
    } // End of findBestEmptyCell()

    // Describes the first rule violation among the given digits, or "" if none.  // Describes firstConflict()
    std::string firstConflict() const
    {                       // Used to detect contradictory puzzles up front
        Board copy = *this; // Work on a copy so this Board is not modified
        for (int r = 0; r < SIZE; ++r)
        { // Check every row
            for (int c = 0; c < SIZE; ++c)
            {                            // Check every column
                int v = copy.grid[r][c]; // The digit in this cell
                if (v == 0)
                    continue;                                                                      // Empty cells cannot conflict
                copy.grid[r][c] = 0;                                                               // Temporarily remove the digit
                bool ok = copy.canPlace(r, c, v);                                                  // Could it be placed again without a clash?
                copy.grid[r][c] = v;                                                               // Put the digit back
                if (!ok)                                                                           // A clash means two equal digits share a row, column or block
                    return "the given " + std::to_string(v) + " at row " + std::to_string(r + 1) + // Build a human-readable message
                           ", column " + std::to_string(c + 1) +                                   // Add the column (1-based)
                           " conflicts with another " + std::to_string(v) +                        // Say which digit clashes
                           " in its row, column or block";                                         // Say where the clash is
            } // End of column loop
        } // End of row loop
        return ""; // No conflicts found
    } // End of firstConflict()

    void print(std::ostream &out) const
    { // Print the grid with block separators
        for (int r = 0; r < SIZE; ++r)
        { // One output line per row
            if (r > 0 && r % BLOCK == 0)
                out << "------+-------+------\n"; // Horizontal divider between blocks
            for (int c = 0; c < SIZE; ++c)
            { // Print each cell in the row
                if (c > 0 && c % BLOCK == 0)
                    out << "| ";                                                      // Vertical divider between blocks
                out << (grid[r][c] == 0 ? '_' : static_cast<char>('0' + grid[r][c])); // '_' for empty, else the digit
                if (c < SIZE - 1)
                    out << ' '; // Space between cells (not after the last)
            } // End of column loop
            out << '\n'; // End the row
        } // End of row loop
    } // End of print()

    // Assignment 5 revision (Yabsira Tamirat): print the grid in the  // Who wrote this block
    // same format as puzzle1.txt - puzzle5.txt (9 rows, cells separated by spaces).  // What the block does
    void printPlain(std::ostream &out) const
    { // Print without block dividers
        for (int r = 0; r < SIZE; ++r)
        { // One output line per row
            for (int c = 0; c < SIZE; ++c)
            {                                                                         // Each cell in the row
                out << (grid[r][c] == 0 ? '_' : static_cast<char>('0' + grid[r][c])); // '_' for empty, else the digit
                if (c < SIZE - 1)
                    out << ' '; // Single space between cells, none at the end
            } // End of column loop
            out << '\n'; // End the row
        } // End of row loop
    } // End of printPlain()

private:                                          // Private data follows
    std::array<std::array<int, SIZE>, SIZE> grid; // The 9x9 grid of digits (0 = empty)
}; // End of Board

class SudokuSolver
{                                                                               // Start of the SudokuSolver class
public:                                                                         // Public members follow
    SudokuSolver(const Board &puzzle, std::ostream &screen, std::ostream &file) // Puzzle plus the two places to print
        : working(puzzle), screenOut(screen), fileOut(file)
    {
    } // Copy the puzzle; remember both outputs

    // Runs the search and returns how many solutions were found (0 = none).  // Describes solveAll()
    int solveAll()
    {                                       // Entry point for solving
        solutionCount = 0;                  // Start counting from zero
        conflict = working.firstConflict(); // Look for contradictory givens before searching
        if (conflict.empty())
            search();         // Only search when the givens are consistent
        return solutionCount; // Hand back the total
    } // End of solveAll()

    const std::string &getConflict() const { return conflict; } // Message about contradictory givens ("" if none)

private:                     // Private data follows
    Board working;           // The board being modified during the search
    std::ostream &screenOut; // Console output (Assignment 4 report)
    std::ostream &fileOut;   // solutionN.txt output (Assignment 5)
    int solutionCount = 0;   // How many solutions have been found so far
    std::string conflict;    // Describes contradictory givens, if any

    // Recursive step: fill one empty cell, recurse, then undo (backtrack).  // Describes search()
    void search()
    {                     // The recursive depth-first search
        int r = 0, c = 0; // Will hold the chosen cell
        if (!working.findBestEmptyCell(r, c))
        {                     // No empty cells left?
            recordSolution(); // Then the grid is a complete solution; print it now
            return;           // Back up to try other possibilities
        } // End of base case

        for (int v = 1; v <= Board::SIZE; ++v)
        { // Try each digit 1-9 in the chosen cell
            if (!working.canPlace(r, c, v))
                continue;         // Skip digits that break a rule
            working.set(r, c, v); // Place the candidate
            search();             // Recurse to fill the remaining cells
            working.set(r, c, 0); // Backtrack: remove the candidate
        } // End of digit loop (no legal digit means a dead end)
    } // End of search()

    // Prints the finished grid to the screen and to the solution file.  // Describes recordSolution()
    void recordSolution()
    {                                                       // Called once per complete solution
        ++solutionCount;                                    // One more solution found
        screenOut << "Solution " << solutionCount << ":\n"; // Numbered heading on the screen
        working.print(screenOut);                           // Screen copy with block dividers
        screenOut << '\n';                                  // Blank line after the grid
        if (solutionCount > 1)
            fileOut << '\n';         // Blank line between solutions in the file
        working.printPlain(fileOut); // File copy in the puzzle-file format
    } // End of recordSolution()
}; // End of SudokuSolver

class PuzzleRunner
{                                                                      // Start of the PuzzleRunner class
public:                                                                // Public members follow
    explicit PuzzleRunner(const std::string &file) : filename(file) {} // Remember which file to process

    // Returns true if the file was read and processed, false on a read error.  // Describes run()
    bool run(std::ostream &out) const
    {                                                 // Process the file and print to 'out'
        out << "==============================\n";    // Visual separator between puzzles
        out << "Puzzle file: " << filename << "\n\n"; // Always show the file name first

        Board puzzle; // Will hold the puzzle read from the file
        try
        {                          // Catch any problem so one bad file cannot crash the program
            puzzle.load(filename); // Read and validate the file
        }
        catch (const PuzzleException &e)
        {                                           // Expected, descriptive file errors
            out << "ERROR: " << e.what() << "\n\n"; // Show the specific reason
            return false;                           // Tell the caller this file failed
        }
        catch (const std::exception &e)
        {                                                                             // Anything unexpected (e.g. out of memory)
            out << "ERROR: unexpected problem while reading: " << e.what() << "\n\n"; // Show what we know
            return false;                                                             // Tell the caller this file failed
        } // End of try/catch

        out << "Puzzle:\n"; // Heading for the original puzzle
        puzzle.print(out);  // Print the puzzle as read from the file
        out << '\n';        // Blank line for readability

        // Assignment 5 revision (Yabsira Tamirat): open solutionN.txt  // Who wrote this block
        // before solving so each solution can be written the moment it is found.  // Why the file opens here
        std::string solutionName = solutionFileName(); // e.g. "solution3.txt" for "puzzle3.txt"
        std::ofstream solutionFile(solutionName);      // Create (or overwrite) the solution file
        if (!solutionFile)
        {                                                                // Edge case: the file could not be created
            out << "ERROR: could not create " << solutionName << "\n\n"; // Tell the user
            return false;                                                // Count this file as a failure
        } // End of open check

        try
        {                                                   // Guard the solving step as well
            SudokuSolver solver(puzzle, out, solutionFile); // Solver prints to the screen and the file
            int count = solver.solveAll();                  // Find (and print) every solution

            if (count == 0)
            {                                                          // No solution exists
                out << "No solution found\n";                          // Required message from the assignment
                if (!solver.getConflict().empty())                     // If the givens were contradictory...
                    out << "Reason: " << solver.getConflict() << "\n"; // ...explain why
                solutionFile << "No solution found.\n";                // Required single line in the file
            }
            else
            {                                                         // At least one solution was printed
                out << "Solutions found: " << count << "\n";          // Total on the screen
                solutionFile << "Solutions found: " << count << "\n"; // Required count line in the file
            } // End of result check

            if (!solutionFile)
                out << "ERROR: failed while writing " << solutionName << "\n\n"; // Edge case: write error
            else
                out << "Results written to " << solutionName << "\n\n"; // Confirm where the results went
        }
        catch (const std::exception &e)
        {                                                                             // Unexpected failure while solving
            out << "ERROR: unexpected problem while solving: " << e.what() << "\n\n"; // Show what we know
            return false;                                                             // Tell the caller this file failed
        } // End of try/catch
        return true; // File processed successfully
    } // End of run()

private:                  // Private data follows
    std::string filename; // The puzzle file this runner handles

    // Assignment 5 revision (Yabsira Tamirat): build the output file  // Who wrote this block
    // name. "puzzle3.txt" (or "dir/puzzle3.txt") becomes "solution3.txt".  // What the block does
    std::string solutionFileName() const
    {                                            // Returns the name of the solution file
        std::string base = filename;             // Start from the puzzle file name
        size_t slash = base.find_last_of("/\\"); // Find the last folder separator, if any
        if (slash != std::string::npos)
            base = base.substr(slash + 1);                                             // Keep only the file's own name
        size_t dot = base.find_last_of('.');                                           // Find the start of the extension
        std::string stem = (dot == std::string::npos) ? base : base.substr(0, dot);    // Name without ".txt"
        size_t start = stem.size();                                                    // Begin at the end of the stem
        while (start > 0 && std::isdigit(static_cast<unsigned char>(stem[start - 1]))) // Walk back over trailing digits
            --start;                                                                   // Include this digit in the puzzle number
        std::string number = stem.substr(start);                                       // The puzzle number N (may be empty)
        if (number.empty())
            return "solution_" + stem + ".txt"; // No number in the name: fall back to the stem
        return "solution" + number + ".txt";    // Normal case: solutionN.txt
    } // End of solutionFileName()
}; // End of PuzzleRunner

class Application
{       // Start of the Application class
public: // Public members follow
    Application(int argc, char *argv[])
    { // Build the list of files from the command line
        if (argc > 1)
        { // The user passed file names
            for (int i = 1; i < argc; ++i)
                files.push_back(argv[i]); // Use each argument as a file name
        }
        else
        {                                                               // No arguments given
            for (int i = 1; i <= 5; ++i)                                // Default to puzzle1.txt .. puzzle5.txt
                files.push_back("puzzle" + std::to_string(i) + ".txt"); // Build each default file name
        } // End of argument handling
    } // End of constructor

    int run() const
    {                                      // Process every file and return the exit code
        int failures = 0;                  // Count of files that could not be read
        for (const std::string &f : files) // Go through each file in order
            if (!PuzzleRunner(f).run(std::cout))
                ++failures;                                                // Run it and count any failure
        if (failures > 0)                                                  // Summarise problems at the end
            std::cout << failures << " file(s) could not be processed.\n"; // Tell the user how many failed
        return failures > 0 ? 1 : 0;                                       // Non-zero exit code signals an error to the shell
    } // End of run()

private:                            // Private data follows
    std::vector<std::string> files; // The list of puzzle files to process
}; // End of Application

int main(int argc, char *argv[])
{ // Program entry point
    try
    {                                         // Last line of defence against uncaught exceptions
        return Application(argc, argv).run(); // Create the application and run it
    }
    catch (const std::exception &e)
    {                                                     // Any exception that escaped everything else
        std::cerr << "Fatal error: " << e.what() << '\n'; // Report it on the error stream
        return 2;                                         // Exit code 2 means a fatal error
    } // End of try/catch
} // End of main
