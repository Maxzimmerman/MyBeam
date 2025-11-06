defmodule Mix.Tasks.Printer do
  use Mix.Task

  @shortdoc "Prints BEAM file info"

  def run([path]) do
    print(path)
  end

  def print(path) do
    chunks_to_fetch = [:atoms, :imports, :exports, :attributes, :literals]

    {:ok, {module, chunks}} = :beam_lib.chunks(~c"#{path}", chunks_to_fetch)

    IO.puts("MODULE:")
    IO.inspect(module)

    IO.puts("\nCHUNKS:")
    IO.inspect(chunks)
  end
end
