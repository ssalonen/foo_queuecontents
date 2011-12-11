
Settings.MinSimilarity = 0.90
wait("TESTTESTTEST.png")

row_h = 21.3333
firstCol = find("Default.png")
firstCol = firstCol.moveTo(firstCol.getTopLeft().below(int(row_h*2.0)))
# Activate first column
click(firstCol.getBottomRight())

#ctrl+c ctrl+v
type('c', KeyModifier.CTRL)
type('v', KeyModifier.CTRL)

def assert_order():
    mouseMove(firstCol)
    tmp = Settings.MinSimilarity
    Settings.MinSimilarity = 0.99
    assert exists("BCON-1.png"), 'B column messed up!'
    assert exists("ACON-1.png"), 'A column messed up!'
    Settings.MinSimilarity = tmp


def get_row_region(index=0):
    tmp = Region(firstCol)
    return tmp.moveTo(tmp.getTopLeft().below(int(index*row_h)))

def remove_all_from_queue():
    wait(0.5)
    click(get_row_region(0).getBottomRight())
    type('a', KeyModifier.CTRL)
    type('q', KeyModifier.SHIFT + KeyModifier.CTRL)

def add_to_queue(index=0):
    wait(0.5)
    #get_row_region(index).highlight(0.2)
    click(get_row_region(0).getBottomRight())
    for i in range(index): type(Key.DOWN)
    type('q', KeyModifier.CTRL)
        
def remove_from_queue(index=1):
    wait(0.5)
    #get_row_region(index).highlight(0.2)
    click(get_row_region(0).getBottomRight())
    for i in range(index): type(Key.DOWN)
    type('q', KeyModifier.SHIFT + KeyModifier.CTRL)

def drag_drop_rows(start_index, end_index):    
    reg = get_row_region(start_index)
    tmp = Settings.MoveMouseDelay

    dir = -1 if start_index - end_index > 0 else 1
    Settings.MoveMouseDelay = 0.5
    wait(0.5)
    click(reg)
    wait(0.5)
    dragDrop(reg, get_row_region(end_index).below(int(dir*row_h)))
    Settings.MoveMouseDelay = tmp


remove_all_from_queue()

# Initialize
Settings.MoveMouseDelay = 0.1
testCol = find("TESTTESTTEST.png")

# Hide all columns except TEST column
#
rightClick(testCol)

testcol_enabled = find("TESTTESTTEST-1.png")
other_columns_enabled = testcol_enabled.above(400)
assert testcol_enabled, 'test column must be visible'

all_columns = sorted(other_columns_enabled.findAll("1320779565117.png"),
                      key=lambda m: m.y)
all_columns = all_columns[1:] # remove hide header
for checked_column in all_columns:
    rightClick(testCol)
    click(checked_column)


# Show A and B cols
# + Album
rightClick(testCol)
click("1320781085864.png")
rightClick(testCol)
click("1320781103605.png")
rightClick(testCol)
click("Album.png")

# Extend cols
add_to_queue(0)
doubleClick("1320781277778.png")
doubleClick(Pattern("Albu.png").targetOffset(-7,0))

remove_all_from_queue()


add_to_queue(1)
add_to_queue(2)
add_to_queue(5)

assert_order()
drag_drop_rows(3, 1)
assert_order()

# Reorder columns
# Reorder rows

assert_order()
dragDrop("TESTTESTTEST.png", "1320779050872-1.png")
assert_order()
drag_drop_rows(1, 3)

assert_order()

print "TESTS PASSED!"


