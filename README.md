# Aegis-Project
A fast, terminal-based study dashboard and task manager written in C++. 
Features a True Color UI, raw mode countdown timers, and persistent data saving.

## How to Build
Compile the source code using g++:
\`\`\`bash
g++ main.cpp -o aegis
\`\`\`

## How to Run
\`\`\`bash
./aegis
\`\`\`

## Commands
* \`study <min>\` - Log study minutes
* \`timer <val>[s/m/h]\` - Start a countdown timer
* \`add <task>\` - Add a new task to the queue
* \`done <id>\` - Mark a task as completed
* \`exit\` - Safely save and close Aegis
