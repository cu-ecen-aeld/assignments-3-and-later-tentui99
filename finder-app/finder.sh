if [ $# -ne 2 ]; then
    echo "Error, please provide exactly two arguments"
    exit 1
fi

filesdir=$1
searchstr=$2

# Check if filesdir is not a folder
if [ ! -d "$filesdir" ]; then
    echo "Error: $filesdir is not a directory"
    exit 1
fi

X=$(find "$filesdir" -type f | wc -l)
Y=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

echo "The number of files are $X and the number of matching lines are $Y"
