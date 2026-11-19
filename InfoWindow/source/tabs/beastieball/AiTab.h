namespace AiTab {

void AiTab(bool *open);

void AiHooks();

void Undo(const RValue &game_active, int found_ai, bool reset_round = true);
int FindAi(const RValue &game_active, bool always_return_ai = false);

void Store();

}