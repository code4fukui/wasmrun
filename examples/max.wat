(module
  (func (export "max") (param i32 i32) (result i32)
    (local i32)
    local.get 0
    local.get 1
    i32.gt_s
    if
      local.get 0
      local.set 2
    else
      local.get 1
      local.set 2
    end
    local.get 2))
