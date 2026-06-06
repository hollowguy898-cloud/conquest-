#!/usr/bin/env python3
"""
CONQUEST Training Data Loader
Loads binary episode files produced by conquest_train.

Usage:
    from train_loader import ConquestDataset, load_episode

    # Load a single episode file
    examples = load_episode("training_data/episode_000001.bin")
    for ex in examples:
        features = ex["features"]   # np.ndarray, shape (3568,)
        policy   = ex["policy"]     # np.ndarray, shape (1018,)
        value    = ex["value"]      # float
        player   = ex["current_player"]  # int (1 or 2)
        turn     = ex["turn"]       # int

    # Load all episodes from a directory
    dataset = ConquestDataset("training_data/")
    print(f"Loaded {len(dataset)} examples from {dataset.num_episodes} episodes")

    # Use with PyTorch DataLoader
    from torch.utils.data import DataLoader
    loader = DataLoader(dataset, batch_size=64, shuffle=True)

    # Iterate
    for batch in loader:
        features = batch["features"]  # (B, 3568)
        policy   = batch["policy"]    # (B, 1018)
        value    = batch["value"]     # (B,)
"""

import struct
import os
import glob
import numpy as np
from typing import List, Dict, Optional, Tuple

# ============================================================================
# Constants (must match C++ definitions)
# ============================================================================
MAGIC = b"CNQT"
VERSION = 1
HEADER_SIZE = 64

HEX_COUNT = 127
TERRAIN_CHANNELS = 5
PER_HEX_CHANNELS = 28
GLOBAL_FEATURES = 12
FEATURE_SIZE = HEX_COUNT * PER_HEX_CHANNELS + GLOBAL_FEATURES  # 3568
POLICY_SIZE = 1018

POLICY_MOVE_START = 0
POLICY_MOVE_END = 127
POLICY_ATTACK_START = 127
POLICY_ATTACK_END = 254
POLICY_SPAWN_START = 254
POLICY_SPAWN_END = 254 + 5 * 127  # 889
POLICY_CAPTURE_START = 889
POLICY_CAPTURE_END = 889 + 127  # 1016
POLICY_FORTIFY = 1016
POLICY_END_TURN = 1017

# Size of one training example in bytes
# features (FEATURE_SIZE * 4) + policy (POLICY_SIZE * 4) + value (4) +
# current_player (4) + hash (8) + turn (4)
EXAMPLE_BYTES = (FEATURE_SIZE + POLICY_SIZE) * 4 + 4 + 4 + 8 + 4


class EpisodeHeader:
    """Parsed header from a training data file."""

    def __init__(self):
        self.magic = b""
        self.version = 0
        self.feature_size = 0
        self.policy_size = 0
        self.example_count = 0
        self.winner = 0
        self.turns = 0
        self.seed = 0

    @property
    def is_valid(self):
        return self.magic == MAGIC and self.version == VERSION


def parse_header(data: bytes) -> EpisodeHeader:
    """Parse a 64-byte header from binary data."""
    h = EpisodeHeader()
    h.magic = data[0:4]
    h.version = struct.unpack_from("<I", data, 4)[0]
    h.feature_size = struct.unpack_from("<I", data, 8)[0]
    h.policy_size = struct.unpack_from("<I", data, 12)[0]
    h.example_count = struct.unpack_from("<I", data, 16)[0]
    h.winner = struct.unpack_from("<i", data, 20)[0]
    h.turns = struct.unpack_from("<i", data, 24)[0]
    h.seed = struct.unpack_from("<I", data, 28)[0]
    return h


def load_episode(path: str) -> List[Dict]:
    """
    Load a single episode file and return a list of example dicts.

    Each dict contains:
        features:       np.ndarray, shape (FEATURE_SIZE,)
        policy:         np.ndarray, shape (POLICY_SIZE,)
        value:          float
        current_player: int (1 or 2)
        hash:           int (uint64)
        turn:           int
    """
    examples = []

    with open(path, "rb") as f:
        header_data = f.read(HEADER_SIZE)
        if len(header_data) < HEADER_SIZE:
            raise ValueError(f"File too small for header: {path}")

        header = parse_header(header_data)
        if not header.is_valid:
            raise ValueError(f"Invalid header in {path}: magic={header.magic}, version={header.version}")

        if header.feature_size != FEATURE_SIZE:
            raise ValueError(
                f"Feature size mismatch in {path}: "
                f"expected {FEATURE_SIZE}, got {header.feature_size}"
            )
        if header.policy_size != POLICY_SIZE:
            raise ValueError(
                f"Policy size mismatch in {path}: "
                f"expected {POLICY_SIZE}, got {header.policy_size}"
            )

        for _ in range(header.example_count):
            # Read features
            feat_data = f.read(FEATURE_SIZE * 4)
            features = np.frombuffer(feat_data, dtype=np.float32).copy()

            # Read policy
            pol_data = f.read(POLICY_SIZE * 4)
            policy = np.frombuffer(pol_data, dtype=np.float32).copy()

            # Read value
            val_data = f.read(4)
            value = struct.unpack("<f", val_data)[0]

            # Read current_player
            cp_data = f.read(4)
            current_player = struct.unpack("<i", cp_data)[0]

            # Read hash
            hash_data = f.read(8)
            hash_val = struct.unpack("<Q", hash_data)[0]

            # Read turn
            turn_data = f.read(4)
            turn = struct.unpack("<i", turn_data)[0]

            examples.append({
                "features": features,
                "policy": policy,
                "value": value,
                "current_player": current_player,
                "hash": hash_val,
                "turn": turn,
            })

    return examples


def load_episode_metadata(path: str) -> EpisodeHeader:
    """Load only the header from an episode file (no examples)."""
    with open(path, "rb") as f:
        header_data = f.read(HEADER_SIZE)
        if len(header_data) < HEADER_SIZE:
            raise ValueError(f"File too small for header: {path}")
        return parse_header(header_data)


class ConquestDataset:
    """
    Dataset that loads all episode files from a directory.

    Supports indexing and iteration. Can be used with PyTorch DataLoader.
    """

    def __init__(self, directory: str, max_files: Optional[int] = None,
                 filter_winner: Optional[int] = None,
                 min_turn: int = 0, max_turn: int = 999):
        """
        Args:
            directory: Path to directory containing episode_*.bin files
            max_files: Maximum number of files to load (None = all)
            filter_winner: Only load episodes where winner == this (0=draw, 1=P1, 2=P2)
            min_turn: Skip examples from before this turn
            max_turn: Skip examples from after this turn
        """
        self.directory = directory
        self.examples = []
        self.num_episodes = 0
        self.p1_wins = 0
        self.p2_wins = 0
        self.draws = 0

        # Find all episode files
        pattern = os.path.join(directory, "episode_*.bin")
        files = sorted(glob.glob(pattern))

        if max_files is not None:
            files = files[:max_files]

        print(f"Loading {len(files)} episode files from {directory}...")

        for filepath in files:
            try:
                # Check header first for filtering
                header = load_episode_metadata(filepath)
                if filter_winner is not None and header.winner != filter_winner:
                    continue

                # Load full episode
                episode = load_episode(filepath)
                self.num_episodes += 1

                if header.winner == 1:
                    self.p1_wins += 1
                elif header.winner == 2:
                    self.p2_wins += 1
                else:
                    self.draws += 1

                # Filter by turn range
                for ex in episode:
                    if ex["turn"] < min_turn or ex["turn"] > max_turn:
                        continue
                    self.examples.append(ex)

            except Exception as e:
                print(f"Warning: Failed to load {filepath}: {e}")
                continue

        print(f"Loaded {len(self.examples)} examples from {self.num_episodes} episodes")
        print(f"  P1 wins: {self.p1_wins}, P2 wins: {self.p2_wins}, Draws: {self.draws}")

    def __len__(self) -> int:
        return len(self.examples)

    def __getitem__(self, idx: int) -> Dict:
        return self.examples[idx]

    def get_features_array(self) -> np.ndarray:
        """Return all features as a single numpy array, shape (N, FEATURE_SIZE)."""
        return np.stack([ex["features"] for ex in self.examples])

    def get_policy_array(self) -> np.ndarray:
        """Return all policies as a single numpy array, shape (N, POLICY_SIZE)."""
        return np.stack([ex["policy"] for ex in self.examples])

    def get_value_array(self) -> np.ndarray:
        """Return all values as a single numpy array, shape (N,)."""
        return np.array([ex["value"] for ex in self.examples], dtype=np.float32)

    def get_turn_array(self) -> np.ndarray:
        """Return all turn numbers as a single numpy array, shape (N,)."""
        return np.array([ex["turn"] for ex in self.examples], dtype=np.int32)


# ============================================================================
# PyTorch Dataset Wrapper
# ============================================================================
try:
    import torch
    from torch.utils.data import Dataset as TorchDataset

    class ConquestTorchDataset(TorchDataset):
        """
        PyTorch Dataset wrapper for Conquest training data.

        Usage:
            dataset = ConquestTorchDataset("training_data/")
            loader = DataLoader(dataset, batch_size=64, shuffle=True)
            for batch in loader:
                features = batch["features"]  # (B, 3568)
                policy   = batch["policy"]    # (B, 1018)
                value    = batch["value"]      # (B,)
        """

        def __init__(self, directory: str, **kwargs):
            self._dataset = ConquestDataset(directory, **kwargs)

        def __len__(self) -> int:
            return len(self._dataset)

        def __getitem__(self, idx: int) -> Dict[str, torch.Tensor]:
            ex = self._dataset[idx]
            return {
                "features": torch.from_numpy(ex["features"]),
                "policy": torch.from_numpy(ex["policy"]),
                "value": torch.tensor(ex["value"], dtype=torch.float32),
            }

except ImportError:
    # PyTorch not available — ConquestTorchDataset won't be defined
    pass


# ============================================================================
# Feature Channel Index Helper
# ============================================================================
class FeatureChannels:
    """Helper for accessing specific channels from the feature tensor."""

    @staticmethod
    def reshape_to_board(features: np.ndarray) -> np.ndarray:
        """
        Reshape flat features to (PER_HEX_CHANNELS, HEX_COUNT, GLOBAL_FEATURES) layout.

        Returns:
            board_features: shape (HEX_COUNT, PER_HEX_CHANNELS)
            global_features: shape (GLOBAL_FEATURES,)
        """
        board = features[:HEX_COUNT * PER_HEX_CHANNELS].reshape(
            HEX_COUNT, PER_HEX_CHANNELS
        )
        global_feat = features[HEX_COUNT * PER_HEX_CHANNELS:]
        return board, global_feat

    @staticmethod
    def get_terrain_map(features: np.ndarray) -> np.ndarray:
        """Get terrain type per hex. Returns (HEX_COUNT,) int array (0-4)."""
        board, _ = FeatureChannels.reshape_to_board(features)
        return board[:, :TERRAIN_CHANNELS].argmax(axis=1)

    @staticmethod
    def get_influence_maps(features: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """Get P1 and P2 influence. Returns two (HEX_COUNT,) float arrays."""
        board, _ = FeatureChannels.reshape_to_board(features)
        return board[:, 6], board[:, 7]

    @staticmethod
    def get_ownership_maps(features: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """Get P1 and P2 ownership. Returns two (HEX_COUNT,) float arrays."""
        board, _ = FeatureChannels.reshape_to_board(features)
        return board[:, 8], board[:, 9]

    @staticmethod
    def get_unit_maps(features: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """Get unit type per hex for P1 and P2. Returns two (HEX_COUNT,) int arrays."""
        board, _ = FeatureChannels.reshape_to_board(features)
        p1_types = board[:, 10:17].argmax(axis=1)
        p2_types = board[:, 17:24].argmax(axis=1)
        return p1_types, p2_types


# ============================================================================
# Policy Slot Helper
# ============================================================================
class PolicySlots:
    """Helper for interpreting policy vectors."""

    @staticmethod
    def get_move_policy(policy: np.ndarray) -> np.ndarray:
        """Get move probabilities. Returns (127,) array."""
        return policy[POLICY_MOVE_START:POLICY_MOVE_END]

    @staticmethod
    def get_attack_policy(policy: np.ndarray) -> np.ndarray:
        """Get attack probabilities. Returns (127,) array."""
        return policy[POLICY_ATTACK_START:POLICY_ATTACK_END]

    @staticmethod
    def get_spawn_policy(policy: np.ndarray) -> np.ndarray:
        """Get spawn probabilities. Returns (5, 127) array (5 unit types)."""
        raw = policy[POLICY_SPAWN_START:POLICY_SPAWN_END]
        return raw.reshape(5, HEX_COUNT)

    @staticmethod
    def get_capture_policy(policy: np.ndarray) -> np.ndarray:
        """Get capture probabilities. Returns (127,) array."""
        return policy[POLICY_CAPTURE_START:POLICY_CAPTURE_END]

    @staticmethod
    def get_fortify_prob(policy: np.ndarray) -> float:
        """Get fortify probability."""
        return policy[POLICY_FORTIFY]

    @staticmethod
    def get_end_turn_prob(policy: np.ndarray) -> float:
        """Get end turn probability."""
        return policy[POLICY_END_TURN]

    @staticmethod
    def top_actions(policy: np.ndarray, k: int = 5) -> list:
        """Get top-k action probabilities with their slot indices."""
        top_indices = np.argsort(policy)[::-1][:k]
        result = []
        for idx in top_indices:
            prob = policy[idx]
            action_type = "unknown"
            target_hex = -1
            spawn_type = -1

            if POLICY_MOVE_START <= idx < POLICY_MOVE_END:
                action_type = "move"
                target_hex = idx - POLICY_MOVE_START
            elif POLICY_ATTACK_START <= idx < POLICY_ATTACK_END:
                action_type = "attack"
                target_hex = idx - POLICY_ATTACK_START
            elif POLICY_SPAWN_START <= idx < POLICY_SPAWN_END:
                action_type = "spawn"
                offset = idx - POLICY_SPAWN_START
                spawn_type = offset // HEX_COUNT
                target_hex = offset % HEX_COUNT
            elif POLICY_CAPTURE_START <= idx < POLICY_CAPTURE_END:
                action_type = "capture"
                target_hex = idx - POLICY_CAPTURE_START
            elif idx == POLICY_FORTIFY:
                action_type = "fortify"
            elif idx == POLICY_END_TURN:
                action_type = "end_turn"

            result.append({
                "slot": int(idx),
                "prob": float(prob),
                "action": action_type,
                "target_hex": target_hex,
                "spawn_type": spawn_type,
            })
        return result


# ============================================================================
# CLI interface
# ============================================================================
if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="CONQUEST Training Data Inspector")
    parser.add_argument("path", help="Episode file or directory")
    parser.add_argument("--summary", action="store_true", help="Print summary only")
    parser.add_argument("--example", type=int, default=0, help="Example index to inspect")
    args = parser.parse_args()

    if os.path.isdir(args.path):
        dataset = ConquestDataset(args.path)
        if args.summary:
            print(f"\nDataset Summary:")
            print(f"  Examples:    {len(dataset)}")
            print(f"  Episodes:    {dataset.num_episodes}")
            print(f"  P1 wins:     {dataset.p1_wins}")
            print(f"  P2 wins:     {dataset.p2_wins}")
            print(f"  Draws:       {dataset.draws}")
            print(f"  Feature dim: {FEATURE_SIZE}")
            print(f"  Policy dim:  {POLICY_SIZE}")
        else:
            # Inspect a specific example
            if len(dataset) > 0 and args.example < len(dataset):
                ex = dataset[args.example]
                print(f"\nExample {args.example}:")
                print(f"  Value:          {ex['value']:.3f}")
                print(f"  Current player: {ex['current_player']}")
                print(f"  Turn:           {ex['turn']}")
                print(f"  Hash:           {ex['hash']:#018x}")
                print(f"  Feature range:  [{ex['features'].min():.3f}, {ex['features'].max():.3f}]")
                print(f"  Policy range:   [{ex['policy'].min():.3f}, {ex['policy'].max():.3f}]")
                print(f"  Policy sum:     {ex['policy'].sum():.3f}")
                print(f"\n  Top-5 actions:")
                for action in PolicySlots.top_actions(ex["policy"], k=5):
                    print(f"    {action['action']}: prob={action['prob']:.3f} "
                          f"hex={action['target_hex']} "
                          f"spawn_type={action['spawn_type']}")
    else:
        # Single file
        examples = load_episode(args.path)
        header = load_episode_metadata(args.path)
        print(f"\nEpisode: {args.path}")
        print(f"  Examples:  {header.example_count}")
        print(f"  Winner:    {'P1' if header.winner == 1 else 'P2' if header.winner == 2 else 'DRAW'}")
        print(f"  Turns:     {header.turns}")
        print(f"  Seed:      {header.seed}")
