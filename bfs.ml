type cell = WALL | GOAL  | FLOOR
type dir = LEFT | RIGHT | UP | DOWN
type pos = {row : int; col: int}
exception ILLEGALSTATE

module PosSet = Set.Make(struct
  type t = pos
  let compare a b =
    let r = compare a.row b.row in
    if r <> 0 then r else compare a.col b.col
end)

type state = {
  grid:   cell array array;
  player: pos;
  boxes:  PosSet.t;
  goals:  PosSet.t; 
}

module StateSet = Set.Make(struct
  type t = state
  let compare a b =
    let r = compare a.player.row b.player.row in
    if r <> 0 then r
    else
      let c = compare a.player.col b.player.col in
      if c <> 0 then c
      else PosSet.compare a.boxes b.boxes
end)

let add_pos p (dr, dc) = { row = p.row + dr; col = p.col + dc }

let is_solved s = PosSet.equal s.boxes s.goals


let delta (d: dir) : int * int = 
  match d with 
   LEFT  -> (0, -1)
  | RIGHT -> (0,  1)
  | UP    -> (-1, 0)
  | DOWN  -> (1,  0)

let idx_grid grid p = grid.(p.row).(p.col)

let move (s : state) (d : dir) : state option = 
  let dxy = delta(d) in
  let next = add_pos s.player dxy in
  let nextnext = add_pos next dxy in
  let is_box = (PosSet.mem next s.boxes) in
  let wall p = idx_grid s.grid p = WALL 
in 
  if wall next then None 
  else let is_next_box = (PosSet.mem nextnext s.boxes) in 
      if is_box && (wall nextnext || is_next_box) then None
  else if is_box then  
    Some { s with player = next; boxes = s.boxes |> PosSet.remove next |> PosSet.add nextnext }
  else Some {s with player = next}


  let check_and_filter (visited) (state) (path) (d) =
    match move state d with
    | None -> None
    | Some next ->
      if StateSet.mem next visited then None  
      else Some (next, d :: path)


  let bfs (start : state) : (dir list) option = 
    let rec loop q visited = 
      match q with
      | [] -> None
      | (state, path) :: rest ->
        if is_solved state then Some (List.rev path)
          
        else 
          let dirs = [LEFT; RIGHT; UP; DOWN] in
          let neighbors = dirs |> (List.filter_map (check_and_filter visited state path))
          in
          loop
            (rest @ neighbors)
            (StateSet.add state visited)
    in
    loop [(start, [])] StateSet.empty


