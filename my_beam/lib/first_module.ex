defmodule FirstModule do
  @my_string "HELLO"

  def force_atoms do
    :first
    :second
    :third
  end

  def first_func do
    @my_string
    str = @my_string
    IO.puts(str)
  end
end
