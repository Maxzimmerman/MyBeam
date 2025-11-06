defmodule MyBeamTest do
  use ExUnit.Case
  doctest MyBeam

  test "greets the world" do
    assert MyBeam.hello() == :world
  end
end
