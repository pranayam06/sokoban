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

let is_wall s p = idx_grid s.grid p = WALL 
let is_box s p = PosSet.mem p s.boxes

let move (s : state) (d : dir) : ((state option) * (pos option)) = 
  let dxy = delta(d) in
  let next = add_pos s.player dxy in
  let nextnext = add_pos next dxy in
  let is_box = (PosSet.mem next s.boxes) 
in 
  if is_wall s next then (None, None)
  else let is_next_box = (PosSet.mem nextnext s.boxes) in 
      if is_box && (is_wall s nextnext || is_next_box) then (None, None)
  else if is_box then  
    (Some { s with player = next; boxes = s.boxes |> PosSet.remove next |> PosSet.add nextnext }, Some nextnext)
  else (Some {s with player = next}, None) 



  let is_blocked (s: state) (p : pos) (d: dir) = 
    let next = add_pos p (delta d) in
    is_wall s next
    
    
  let is_deadlock (s: state) (box: pos) =
      if PosSet.mem box s.goals then false
      else
        let helper = is_blocked s box in
        (helper LEFT || helper RIGHT) && (helper UP || helper DOWN)

  
  let check_and_filter (visited) (state) (path) (d) =
    match move state d with
    | None, _ -> None
    | (Some next, Some box) ->
      if (StateSet.mem next visited || (is_deadlock next box)) then None  
      else Some (next, d :: path) 
    | (Some next, _) -> 
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
            (List.fold_left (fun v (s, _) -> StateSet.add s v) visited neighbors)
    in
    loop [(start, [])] StateSet.empty



    let bfs2 (start: state) : dir list option =
      let q = Queue.create () in
      Queue.push (start, []) q;
      let visited = ref StateSet.empty in
      let rec loop () =
        if Queue.is_empty q then None
        else
          let (state, path) = Queue.pop q in
          if (StateSet.mem state !visited) then loop ()
          else if is_solved state then Some (List.rev path)
          else begin
            visited := StateSet.add state !visited;
            List.iter (fun d ->
              match move state d with
              | (None, _) -> ()
              | (Some next, Some box) -> 
                if (not (is_deadlock next box)) || (is_solved next) then
                  Queue.push (next, d :: path) q
              | (Some next, None) ->
                Queue.push (next, d :: path) q
            ) [LEFT; RIGHT; UP; DOWN];
            loop ()
          end
      in
      loop ()