
def merge(left, right):
    result = []
    left_count = 0
    right_count = 0
    try:
    # while len(left) > left_count and len(right) > right_count:
        while True:
            if left[left_count] > right[right_count]:
                result.append(right[right_count])
                right_count += 1
            else:
                result.append(left[left_count])
                left_count += 1
    except IndexError:
        return result + left[left_count:] + right[right_count:]

    result += left[left_count:]
    result += right[right_count:]
    return result

def merge_sort(seq):
    if len(seq) == 1:
        return seq
    m = len(seq) // 2
    left = merge_sort(seq[:m])
    right = merge_sort(seq[m:])
    return merge(left, right)


# Driver code to test above 
arr = [int(line) for line in open("t/list.txt")]
print ("Given array is", arr[0:10], "...")
for _ in range(10):
    print ("Sorted array is", merge_sort(arr)[0:10], "...")

