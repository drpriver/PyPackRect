from typing import TypeVar, Sequence, Tuple, List
T = TypeVar('T')
def pack(
    input:Sequence[Tuple[int, int, T]],
    width:int=0,
    height:int=0,
    max_iters:int=10,
) -> Tuple[List[Tuple[int, int, int, int, T]], int, int]:
    ...
