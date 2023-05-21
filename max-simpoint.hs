import Data.Bifunctor
import Data.List
import System.Directory
import System.Environment
import System.FilePath
import System.IO
import Text.Regex.TDFA

main :: IO ()
main = do
  [weightDir] <- getArgs
  weights <- readWeights weightDir
  mapM_ print $
    sortBy (\b1 b2 -> compare (fst b1) (fst b2)) $
      map
        (second $ maximumBy (\(_, w1) (_, w2) -> compare w1 w2))
        weights

benchmarkDirRegex :: String
benchmarkDirRegex = "^[0-9]+\\.[^.]+$"

readWeights :: FilePath -> IO [(String, [(Int, Double)])]
readWeights dir = do
  benchmarks <- filter (=~ benchmarkDirRegex) <$> listDirectory dir
  simpoints <- mapM (\benchmark -> readFile' $ dir </> benchmark </> "simpoints.out") benchmarks
  weights <- mapM (\benchmark -> readFile' $ dir </> benchmark </> "weights.out") benchmarks
  return $
    zipWith3
      ( \benchmark simpoint weight ->
          (benchmark, zip (map read $ lines simpoint) (map read $ lines weight))
      )
      benchmarks
      simpoints
      weights
