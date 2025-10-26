-- Helper: run a command in a reusable bottom terminal, auto-close + auto-scroll
local function run_in_bottom_term(cmd, height)
  height = height or 15
  local curwin = vim.api.nvim_get_current_win()
  local term_win

  -- find or create the bottom terminal window
  for _, win in ipairs(vim.api.nvim_list_wins()) do
    local buf = vim.api.nvim_win_get_buf(win)
    if vim.bo[buf].buftype == "terminal" and vim.w[win].chip8_term then
      term_win = win
      break
    end
  end
  if not term_win then
    vim.cmd(("botright %dsplit"):format(height))
    term_win = vim.api.nvim_get_current_win()
    vim.w[term_win].chip8_term = true
    vim.opt_local.number = false
    vim.opt_local.relativenumber = false
    vim.opt_local.signcolumn = "no"
    vim.opt_local.list = false
    vim.opt_local.winfixheight = true
    pcall(function() vim.treesitter.stop(0) end)
  else
    vim.api.nvim_set_current_win(term_win)
    vim.cmd(("resize %d"):format(height))
  end

  -- fresh terminal buffer each run
  vim.cmd("enew")
  local term_buf = vim.api.nvim_get_current_buf()
  pcall(function() vim.treesitter.stop(term_buf) end)

  -- helper to scroll to bottom if user is near bottom already
  local function autoscroll()
    if not (vim.api.nvim_win_is_valid(term_win) and vim.api.nvim_buf_is_valid(term_buf)) then return end
    vim.schedule(function()
      if not (vim.api.nvim_win_is_valid(term_win) and vim.api.nvim_buf_is_valid(term_buf)) then return end
      local last  = vim.api.nvim_buf_line_count(term_buf)
      local cur   = vim.api.nvim_win_get_cursor(term_win)[1]
      local h     = vim.api.nvim_win_get_height(term_win)
      -- only auto-scroll if the cursor is already near the bottom,
      -- so we don't fight the user when they've scrolled up to read logs
      if cur >= last - h - 2 then
        vim.api.nvim_win_set_cursor(term_win, { last, 0 })
      end
    end)
  end

  vim.fn.termopen(cmd, {
    on_stdout = function() autoscroll() end,
    on_stderr = function() autoscroll() end,
    on_exit = function(_, _)
      -- give the terminal a moment to flush, then close the window
      vim.schedule(function()
        if vim.api.nvim_win_is_valid(term_win) then
          vim.api.nvim_win_close(term_win, true)
        end
      end)
    end,
  })

  -- Optional: allow pressing <Enter> in the term to close immediately
  vim.keymap.set("t", "<CR>", function()
    if vim.api.nvim_win_is_valid(term_win) then
      vim.api.nvim_win_close(term_win, true)
    end
  end, { buffer = term_buf, nowait = true, silent = true })

  -- return focus to your source window
  vim.api.nvim_set_current_win(curwin)
end
--
--
--
--
--
-- F5: Build with CMake in bottom terminal (no focus change)
vim.keymap.set("n", "<F5>", function()
  vim.cmd("w")
  run_in_bottom_term({ "cmake", "--build", "build" }, 15)
end, { desc = "Build project with CMake" })

-- F6: Run program; first arg gets filename <Tab> completion (legacy input form)
vim.keymap.set("n", "<F6>", function()
  -- Make sure command-line completion behaves nicely
  vim.o.wildmenu = true
  vim.o.wildmode = "longest:full,full"

  -- Legacy 3-arg input() with completion = "file"
  local first = vim.fn.input("Program arg (file): ", "", "file")  -- <Tab> completes
  -- local rest  = vim.fn.input("Extra args (optional): ")

  local exe = (package.config:sub(1,1) == "\\") and "build\\chip8.exe" or "build/chip8"

  -- Split extra args (quotes respected in most shells; keep it simple here)
  local args = {}
  if first ~= "" then table.insert(args, first) end
  -- if rest  ~= "" then for a in rest:gmatch("%S+") do table.insert(args, a) end end

  run_in_bottom_term(vim.list_extend({ exe }, args), 15)
  vim.cmd("echo ''")
end, { desc = "Run program (filename completion on first arg)" })
